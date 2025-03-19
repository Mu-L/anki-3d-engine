// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/ShaderCompiler/ShaderCompiler.h>
#include <AnKi/ShaderCompiler/ShaderDump.h>
#include <AnKi/ShaderCompiler/MaliOfflineCompiler.h>
#include <AnKi/ShaderCompiler/RadeonGpuAnalyzer.h>
#include <AnKi/ShaderCompiler/Dxc.h>
#include <AnKi/Util/ThreadHive.h>
#include <AnKi/Util/System.h>
#include <ThirdParty/SpirvCross/spirv.hpp>

using namespace anki;

static const char* kUsage = R"(Dump the shader binary to stdout
Usage: %s [options] input_shader_program_binary
Options:
-stats <0|1>  : Print performance statistics for all shaders. Default 0
-binary <0|1> : Print the whole shader program binary. Default 1
-glsl <0|1>   : Print GLSL. Default 1
-spirv <0|1>  : Print SPIR-V. Default 0
-v            : Verbose log
)";

static Error parseCommandLineArgs(WeakArray<char*> argv, Bool& dumpStats, Bool& dumpBinary, Bool& glsl, Bool& spirv, String& filename)
{
	// Parse config
	if(argv.getSize() < 2)
	{
		return Error::kUserData;
	}

	dumpStats = false;
	dumpBinary = true;
	glsl = true;
	spirv = false;
	filename = argv[argv.getSize() - 1];

	for(U32 i = 1; i < argv.getSize() - 1; i++)
	{
		if(CString(argv[i]) == "-stats")
		{
			++i;
			if(i >= argv.getSize())
			{
				return Error::kUserData;
			}

			if(CString(argv[i]) == "1")
			{
				dumpStats = true;
			}
			else if(CString(argv[i]) == "0")
			{
				dumpStats = false;
			}
			else
			{
				return Error::kUserData;
			}
		}
		else if(CString(argv[i]) == "-binary")
		{
			++i;
			if(i >= argv.getSize())
			{
				return Error::kUserData;
			}

			if(CString(argv[i]) == "1")
			{
				dumpBinary = true;
			}
			else if(CString(argv[i]) == "0")
			{
				dumpBinary = false;
			}
			else
			{
				return Error::kUserData;
			}
		}
		else if(CString(argv[i]) == "-glsl")
		{
			++i;
			if(i >= argv.getSize())
			{
				return Error::kUserData;
			}

			if(CString(argv[i]) == "1")
			{
				glsl = true;
			}
			else if(CString(argv[i]) == "0")
			{
				glsl = false;
			}
			else
			{
				return Error::kUserData;
			}
		}
		else if(CString(argv[i]) == "-spirv")
		{
			++i;
			if(i >= argv.getSize())
			{
				return Error::kUserData;
			}

			if(CString(argv[i]) == "1")
			{
				spirv = true;
			}
			else if(CString(argv[i]) == "0")
			{
				spirv = false;
			}
			else
			{
				return Error::kUserData;
			}
		}
		else if(CString(argv[i]) == "-v")
		{
			Logger::getSingleton().enableVerbosity(true);
		}
	}

	if(spirv || glsl)
	{
		dumpBinary = true;
	}

	return Error::kNone;
}

Error dumpStats(const ShaderBinary& bin)
{
	printf("\nOffline compilers stats:\n");
	fflush(stdout);

	class Stats
	{
	public:
		class
		{
		public:
			F64 m_fma;
			F64 m_cvt;
			F64 m_sfu;
			F64 m_loadStore;
			F64 m_varying;
			F64 m_texture;
			F64 m_workRegisters;
			F64 m_fp16ArithmeticPercentage;
			F64 m_spillingCount;
		} m_arm;

		class
		{
		public:
			F64 m_vgprCount;
			F64 m_sgprCount;
			F64 m_isaSize;
		} m_amd;

		Stats(F64 v)
		{
			m_arm.m_fma = m_arm.m_cvt = m_arm.m_sfu = m_arm.m_loadStore = m_arm.m_varying = m_arm.m_texture = m_arm.m_workRegisters =
				m_arm.m_fp16ArithmeticPercentage = m_arm.m_spillingCount = v;

			m_amd.m_vgprCount = m_amd.m_sgprCount = m_amd.m_isaSize = v;
		}

		Stats()
			: Stats(0.0)
		{
		}

		void op(const Stats& b, void (*func)(F64& a, F64 b))
		{
			func(m_arm.m_fma, b.m_arm.m_fma);
			func(m_arm.m_cvt, b.m_arm.m_cvt);
			func(m_arm.m_sfu, b.m_arm.m_sfu);
			func(m_arm.m_loadStore, b.m_arm.m_loadStore);
			func(m_arm.m_varying, b.m_arm.m_varying);
			func(m_arm.m_texture, b.m_arm.m_texture);
			func(m_arm.m_workRegisters, b.m_arm.m_workRegisters);
			func(m_arm.m_fp16ArithmeticPercentage, b.m_arm.m_fp16ArithmeticPercentage);
			func(m_arm.m_spillingCount, b.m_arm.m_spillingCount);

			func(m_amd.m_vgprCount, b.m_amd.m_vgprCount);
			func(m_amd.m_sgprCount, b.m_amd.m_sgprCount);
			func(m_amd.m_isaSize, b.m_amd.m_isaSize);
		}
	};

	class StageStats
	{
	public:
		Stats m_avgStats{0.0};
		Stats m_maxStats{-1.0};
		Stats m_minStats{kMaxF64};
		U32 m_spillingCount = 0;
		U32 m_count = 0;
	};

	class Ctx
	{
	public:
		DynamicArray<Stats> m_spirvStats;
		DynamicArray<Atomic<U32>> m_spirvVisited;
		Atomic<U32> m_variantCount = {0};
		const ShaderBinary* m_bin = nullptr;
		Atomic<I32> m_error = {0};
	};

	Ctx ctx;
	ctx.m_bin = &bin;
	ctx.m_spirvStats.resize(bin.m_codeBlocks.getSize());
	ctx.m_spirvVisited.resize(bin.m_codeBlocks.getSize(), 0);
	memset(ctx.m_spirvVisited.getBegin(), 0, ctx.m_spirvVisited.getSizeInBytes());

	ThreadHive hive(getCpuCoresCount());

	ThreadHiveTaskCallback callback = [](void* userData, [[maybe_unused]] U32 threadId, [[maybe_unused]] ThreadHive& hive,
										 [[maybe_unused]] ThreadHiveSemaphore* signalSemaphore) {
		Ctx& ctx = *static_cast<Ctx*>(userData);
		U32 variantIdx;

		while((variantIdx = ctx.m_variantCount.fetchAdd(1)) < ctx.m_bin->m_variants.getSize() && ctx.m_error.load() == 0)
		{
			const ShaderBinaryVariant& variant = ctx.m_bin->m_variants[variantIdx];

			for(U32 t = 0; t < variant.m_techniqueCodeBlocks.getSize(); ++t)
			{
				for(ShaderType shaderType : EnumBitsIterable<ShaderType, ShaderTypeBit>(ctx.m_bin->m_techniques[t].m_shaderTypes))
				{
					const U32 codeblockIdx = variant.m_techniqueCodeBlocks[t].m_codeBlockIndices[shaderType];

					const Bool visited = ctx.m_spirvVisited[codeblockIdx].fetchAdd(1) != 0;
					if(visited)
					{
						continue;
					}

					const ShaderBinaryCodeBlock& codeBlock = ctx.m_bin->m_codeBlocks[codeblockIdx];

					// Rewrite spir-v because of the decorations we ask DXC to put
					Bool bRequiresMeshShaders = false;
					DynamicArray<U8> newSpirv;
					newSpirv.resize(codeBlock.m_binary.getSize());
					memcpy(newSpirv.getBegin(), codeBlock.m_binary.getBegin(), codeBlock.m_binary.getSizeInBytes());
					visitSpirv(WeakArray<U32>(reinterpret_cast<U32*>(newSpirv.getBegin()), U32(newSpirv.getSizeInBytes() / sizeof(U32))),
							   [&](U32 cmd, WeakArray<U32> instructions) {
								   if(cmd == spv::OpDecorate && instructions[1] == spv::DecorationDescriptorSet
									  && instructions[2] == kDxcVkBindlessRegisterSpace)
								   {
									   // Bindless set, rewrite its set
									   instructions[2] = kMaxRegisterSpaces;
								   }
								   else if(cmd == spv::OpCapability && instructions[0] == spv::CapabilityMeshShadingEXT)
								   {
									   bRequiresMeshShaders = true;
								   }
							   });

					// Arm stats
					MaliOfflineCompilerOut maliocOut;
					Error err = Error::kNone;

					if((shaderType == ShaderType::kVertex || shaderType == ShaderType::kPixel || shaderType == ShaderType::kCompute)
					   && !bRequiresMeshShaders)
					{
						err = runMaliOfflineCompiler(newSpirv, shaderType, maliocOut);

						if(err)
						{
							ANKI_LOGE("Mali offline compiler failed");
							ctx.m_error.store(1);
							break;
						}
					}

					// AMD
					RgaOutput rgaOut = {};
#if 1
					if((shaderType == ShaderType::kVertex || shaderType == ShaderType::kPixel || shaderType == ShaderType::kCompute)
					   && !bRequiresMeshShaders)
					{
						err = runRadeonGpuAnalyzer(newSpirv, shaderType, rgaOut);
						if(err)
						{
							ANKI_LOGE("Radeon GPU Analyzer compiler failed");
							ctx.m_error.store(1);
							break;
						}
					}
#endif

					// Write stats
					Stats& stats = ctx.m_spirvStats[codeblockIdx];

					stats.m_arm.m_fma = maliocOut.m_fma;
					stats.m_arm.m_cvt = maliocOut.m_cvt;
					stats.m_arm.m_sfu = maliocOut.m_sfu;
					stats.m_arm.m_loadStore = maliocOut.m_loadStore;
					stats.m_arm.m_varying = maliocOut.m_varying;
					stats.m_arm.m_texture = maliocOut.m_texture;
					stats.m_arm.m_workRegisters = maliocOut.m_workRegisters;
					stats.m_arm.m_fp16ArithmeticPercentage = maliocOut.m_fp16ArithmeticPercentage;
					stats.m_arm.m_spillingCount = (maliocOut.m_spilling) ? 1.0 : 0.0;

					stats.m_amd.m_vgprCount = F64(rgaOut.m_vgprCount);
					stats.m_amd.m_sgprCount = F64(rgaOut.m_sgprCount);
					stats.m_amd.m_isaSize = F64(rgaOut.m_isaSize);
				}

				if(variantIdx > 0 && ((variantIdx + 1) % 32) == 0)
				{
					printf("Processed %u out of %u variants\n", variantIdx + 1, ctx.m_bin->m_variants.getSize());
				}
			}
		} // while
	};

	for(U32 i = 0; i < hive.getThreadCount(); ++i)
	{
		hive.submitTask(callback, &ctx);
	}

	hive.waitAllTasks();

	if(ctx.m_error.load() != 0)
	{
		return Error::kFunctionFailed;
	}

	// Cather the results
	Array<StageStats, U32(ShaderType::kCount)> allStageStats;
	for(const ShaderBinaryVariant& variant : bin.m_variants)
	{
		for(U32 t = 0; t < variant.m_techniqueCodeBlocks.getSize(); ++t)
		{
			for(ShaderType shaderType : EnumBitsIterable<ShaderType, ShaderTypeBit>(ctx.m_bin->m_techniques[t].m_shaderTypes))
			{
				const U32 codeblockIdx = variant.m_techniqueCodeBlocks[t].m_codeBlockIndices[shaderType];

				const Stats& stats = ctx.m_spirvStats[codeblockIdx];
				StageStats& allStats = allStageStats[shaderType];

				++allStats.m_count;

				allStats.m_avgStats.op(stats, [](F64& a, F64 b) {
					a += b;
				});

				allStats.m_minStats.op(stats, [](F64& a, F64 b) {
					a = min(a, b);
				});

				allStats.m_maxStats.op(stats, [](F64& a, F64 b) {
					a = max(a, b);
				});
			}
		}
	}

	// Print
	for(ShaderType shaderType : EnumIterable<ShaderType>())
	{
		const StageStats& stage = allStageStats[shaderType];
		if(stage.m_count == 0)
		{
			continue;
		}

		printf("Stage %u\n", U32(shaderType));
		printf("  Arm shaders spilling regs %u\n", stage.m_spillingCount);

		const F64 countf = F64(stage.m_count);

		const Stats& avg = stage.m_avgStats;
		printf("  Average:\n");
		printf("    Arm: Regs %f FMA %f CVT %f SFU %f LS %f VAR %f TEX %f FP16 %f%%\n", avg.m_arm.m_workRegisters / countf, avg.m_arm.m_fma / countf,
			   avg.m_arm.m_cvt / countf, avg.m_arm.m_sfu / countf, avg.m_arm.m_loadStore / countf, avg.m_arm.m_varying / countf,
			   avg.m_arm.m_texture / countf, avg.m_arm.m_fp16ArithmeticPercentage / countf);
		printf("    AMD: VGPR %f SGPR %f ISA size %f\n", avg.m_amd.m_vgprCount / countf, avg.m_amd.m_sgprCount / countf,
			   avg.m_amd.m_isaSize / countf);

		const Stats& maxs = stage.m_maxStats;
		printf("  Max:\n");
		printf("    Arm: Regs %f FMA %f CVT %f SFU %f LS %f VAR %f TEX %f FP16 %f%%\n", maxs.m_arm.m_workRegisters, maxs.m_arm.m_fma,
			   maxs.m_arm.m_cvt, maxs.m_arm.m_sfu, maxs.m_arm.m_loadStore, maxs.m_arm.m_varying, maxs.m_arm.m_texture,
			   maxs.m_arm.m_fp16ArithmeticPercentage);
		printf("    AMD: VGPR %f SGPR %f ISA size %f\n", maxs.m_amd.m_vgprCount, maxs.m_amd.m_sgprCount, maxs.m_amd.m_isaSize);
	}

	return Error::kNone;
}

Error dump(CString fname, Bool bDumpStats, Bool dumpBinary, Bool glsl, Bool spirv)
{
	ShaderBinary* binary;
	ANKI_CHECK(deserializeShaderBinaryFromFile(fname, binary, ShaderCompilerMemoryPool::getSingleton()));

	class Dummy
	{
	public:
		ShaderBinary* m_binary;

		~Dummy()
		{
			ShaderCompilerMemoryPool::getSingleton().free(m_binary);
		}
	} dummy{binary};

	if(dumpBinary)
	{
		ShaderDumpOptions options;
		options.m_writeGlsl = glsl;
		options.m_writeSpirv = spirv;

		ShaderCompilerString txt;
		dumpShaderBinary(options, *binary, txt);

		printf("%s\n", txt.cstr());
	}

	if(bDumpStats)
	{
		ANKI_CHECK(dumpStats(*binary));
	}

	return Error::kNone;
}

ANKI_MAIN_FUNCTION(myMain)
int myMain(int argc, char** argv)
{
	class Dummy
	{
	public:
		~Dummy()
		{
			DefaultMemoryPool::freeSingleton();
			ShaderCompilerMemoryPool::freeSingleton();
		}
	} dummy;

	DefaultMemoryPool::allocateSingleton(allocAligned, nullptr);
	ShaderCompilerMemoryPool::allocateSingleton(allocAligned, nullptr);

	String filename;
	Bool dumpStats;
	Bool dumpBinary;
	Bool glsl;
	Bool spirv;
	if(parseCommandLineArgs(WeakArray<char*>(argv, argc), dumpStats, dumpBinary, glsl, spirv, filename))
	{
		ANKI_LOGE(kUsage, argv[0]);
		return 1;
	}

	const Error err = dump(filename, dumpStats, dumpBinary, glsl, spirv);
	if(err)
	{
		ANKI_LOGE("Can't dump due to an error. Bye");
		return 1;
	}

	return 0;
}
