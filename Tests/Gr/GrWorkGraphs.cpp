// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <Tests/Framework/Framework.h>
#include <Tests/Gr/GrCommon.h>
#include <AnKi/Util/HighRezTimer.h>
#include <AnKi/Gr.h>

using namespace anki;

static void clearSwapchain(CommandBufferPtr cmdb = CommandBufferPtr())
{
	const Bool continueCmdb = cmdb.isCreated();

	TexturePtr presentTex = GrManager::getSingleton().acquireNextPresentableTexture();

	if(!continueCmdb)
	{
		CommandBufferInitInfo cinit;
		cinit.m_flags = CommandBufferFlag::kGeneralWork | CommandBufferFlag::kSmallBatch;
		cmdb = GrManager::getSingleton().newCommandBuffer(cinit);
	}

	const TextureBarrierInfo barrier = {TextureView(presentTex.get(), TextureSubresourceDesc::all()), TextureUsageBit::kNone,
										TextureUsageBit::kRtvDsvWrite};
	cmdb->setPipelineBarrier({&barrier, 1}, {}, {});

	RenderTarget rt;
	rt.m_textureView = TextureView(presentTex.get(), TextureSubresourceDesc::all());
	rt.m_clearValue.m_colorf = {1.0f, F32(rand()) / F32(RAND_MAX), 1.0f, 1.0f};
	cmdb->beginRenderPass({rt});
	cmdb->endRenderPass();

	const TextureBarrierInfo barrier2 = {TextureView(presentTex.get(), TextureSubresourceDesc::all()), TextureUsageBit::kRtvDsvWrite,
										 TextureUsageBit::kPresent};
	cmdb->setPipelineBarrier({&barrier2, 1}, {}, {});

	if(!continueCmdb)
	{
		cmdb->endRecording();
		GrManager::getSingleton().submit(cmdb.get());
	}
}

template<typename TFunc>
static void runBenchmark(U32 iterationCount, U32 iterationsPerCommandBuffer, Bool bBenchmark, TFunc func)
{
	ANKI_ASSERT(iterationCount >= iterationsPerCommandBuffer && (iterationCount % iterationsPerCommandBuffer) == 0);

	FencePtr fence;

	F64 avgCpuTimePerIterationMs = 0.0;
	DynamicArray<TimestampQueryPtr> timestamps;

	const U32 commandBufferCount = iterationCount / iterationsPerCommandBuffer;
	for(U32 icmdb = 0; icmdb < commandBufferCount; ++icmdb)
	{
		CommandBufferPtr cmdb = GrManager::getSingleton().newCommandBuffer(CommandBufferInitInfo(CommandBufferFlag::kGeneralWork));

		TimestampQueryPtr query1 = GrManager::getSingleton().newTimestampQuery();
		cmdb->writeTimestamp(query1.get());
		timestamps.emplaceBack(query1);

		const U64 cpuTimeStart = HighRezTimer::getCurrentTimeUs();
		for(U32 i = 0; i < iterationsPerCommandBuffer; ++i)
		{
			func(*cmdb);
		}

		// clearSwapchain(cmdb);

		TimestampQueryPtr query2 = GrManager::getSingleton().newTimestampQuery();
		cmdb->writeTimestamp(query2.get());
		timestamps.emplaceBack(query2);

		cmdb->endRecording();
		const U64 cpuTimeEnd = HighRezTimer::getCurrentTimeUs();
		avgCpuTimePerIterationMs += (Second(cpuTimeEnd - cpuTimeStart) * 0.001) / Second(iterationCount);

		GrManager::getSingleton().submit(cmdb.get(), {}, (icmdb == commandBufferCount - 1) ? &fence : nullptr);

		// GrManager::getSingleton().swapBuffers();
	}

	const Bool done = fence->clientWait(kMaxSecond);
	ANKI_TEST_EXPECT_EQ(done, true);

	F64 avgTimePerIterationMs = 0.0f;
	for(U32 i = 0; i < timestamps.getSize(); i += 2)
	{
		Second a, b;
		ANKI_TEST_EXPECT_EQ(timestamps[i]->getResult(a), TimestampQueryResult::kAvailable);
		ANKI_TEST_EXPECT_EQ(timestamps[i + 1]->getResult(b), TimestampQueryResult::kAvailable);

		avgTimePerIterationMs += (Second(b - a) * 1000.0) / Second(iterationCount);
	}

	if(bBenchmark)
	{
		ANKI_TEST_LOGI("Benchmark: avg GPU time: %fms, avg CPU time: %fms", avgTimePerIterationMs, avgCpuTimePerIterationMs);
	}
}

void commonInitWg(Bool& bBenchmark, Bool& bWorkgraphs)
{
	bBenchmark = getenv("BENCHMARK") && CString(getenv("BENCHMARK")) == "1";

	[[maybe_unused]] Error err = CVarSet::getSingleton().setMultiple(Array<const Char*, 2>{"WorkGraphs", "1"});

	commonInit(!bBenchmark);

	bWorkgraphs = getenv("WORKGRAPHS") && CString(getenv("WORKGRAPHS")) == "1" && GrManager::getSingleton().getDeviceCapabilities().m_workGraphs;

	ANKI_TEST_LOGI("Testing with BENCHMARK=%u WORKGRAPHS=%u", bBenchmark, bWorkgraphs);
}

ANKI_TEST(Gr, WorkGraphHelloWorld)
{
	// CVarSet::getSingleton().setMultiple(Array<const Char*, 2>{"Device", "1"});
	commonInit();

	{
		const Char* kSrc = R"(
struct FirstNodeRecord
{
	uint3 m_gridSize : SV_DispatchGrid;
	uint m_value;
};

struct SecondNodeRecord
{
	uint3 m_gridSize : SV_DispatchGrid;
	uint m_value;
};

struct ThirdNodeRecord
{
	uint m_value;
};

RWStructuredBuffer<uint> g_buff : register(u0);

[Shader("node")] [NodeLaunch("broadcasting")] [NodeIsProgramEntry] [NodeMaxDispatchGrid(1, 1, 1)] [NumThreads(16, 1, 1)]
void main(DispatchNodeInputRecord<FirstNodeRecord> inp, [MaxRecords(2)] NodeOutput<SecondNodeRecord> secondNode, uint svGroupIndex : SV_GroupIndex)
{
	GroupNodeOutputRecords<SecondNodeRecord> rec = secondNode.GetGroupNodeOutputRecords(2);

	if(svGroupIndex < 2)
	{
		rec[svGroupIndex].m_gridSize = uint3(16, 1, 1);
		rec[svGroupIndex].m_value = inp.Get().m_value;
	}

	rec.OutputComplete();
}

[Shader("node")] [NodeLaunch("broadcasting")] [NumThreads(16, 1, 1)] [NodeMaxDispatchGrid(16, 1, 1)]
void secondNode(DispatchNodeInputRecord<SecondNodeRecord> inp, [MaxRecords(32)] NodeOutput<ThirdNodeRecord> thirdNode,
				uint svGroupIndex : SV_GROUPINDEX)
{
	GroupNodeOutputRecords<ThirdNodeRecord> recs = thirdNode.GetGroupNodeOutputRecords(32);

	recs[svGroupIndex * 2 + 0].m_value = inp.Get().m_value;
	recs[svGroupIndex * 2 + 1].m_value = inp.Get().m_value;

	recs.OutputComplete();
}

[Shader("node")] [NodeLaunch("coalescing")] [NumThreads(16, 1, 1)]
void thirdNode([MaxRecords(32)] GroupNodeInputRecords<ThirdNodeRecord> inp, uint svGroupIndex : SV_GroupIndex)
{
	if (svGroupIndex * 2 < inp.Count())
		InterlockedAdd(g_buff[0], inp[svGroupIndex * 2].m_value);

	if (svGroupIndex * 2 + 1 < inp.Count())
		InterlockedAdd(g_buff[0], inp[svGroupIndex * 2 + 1].m_value);
}
)";

		ShaderPtr shader = createShader(kSrc, ShaderType::kWorkGraph);

		ShaderProgramInitInfo progInit;
		progInit.m_workGraph.m_shader = shader.get();
		WorkGraphNodeSpecialization wgSpecialization = {"main", UVec3(4, 1, 1)};
		progInit.m_workGraph.m_nodeSpecializations = ConstWeakArray<WorkGraphNodeSpecialization>(&wgSpecialization, 1);
		ShaderProgramPtr prog = GrManager::getSingleton().newShaderProgram(progInit);

		BufferPtr counterBuff = createBuffer(BufferUsageBit::kAllUav | BufferUsageBit::kCopySource, 0u, 1, "CounterBuffer");

		BufferInitInfo scratchInit("scratch");
		scratchInit.m_size = prog->getWorkGraphMemoryRequirements();
		scratchInit.m_usage = BufferUsageBit::kAllUav;
		BufferPtr scratchBuff = GrManager::getSingleton().newBuffer(scratchInit);

		struct FirstNodeRecord
		{
			UVec3 m_gridSize;
			U32 m_value;
		};

		Array<FirstNodeRecord, 2> records;
		for(U32 i = 0; i < records.getSize(); ++i)
		{
			records[i].m_gridSize = UVec3(4, 1, 1);
			records[i].m_value = (i + 1) * 10;
		}

		CommandBufferPtr cmdb = GrManager::getSingleton().newCommandBuffer(CommandBufferInitInfo(CommandBufferFlag::kSmallBatch));
		cmdb->bindShaderProgram(prog.get());
		cmdb->bindUav(0, 0, BufferView(counterBuff.get()));
		cmdb->dispatchGraph(BufferView(scratchBuff.get()), records.getBegin(), records.getSize(), sizeof(records[0]));
		cmdb->endRecording();

		FencePtr fence;
		GrManager::getSingleton().submit(cmdb.get(), {}, &fence);
		fence->clientWait(kMaxSecond);

		validateBuffer(counterBuff, ConstWeakArray(Array<U32, 1>{122880}));
	}

	commonDestroy();
}

ANKI_TEST(Gr, WorkGraphAmplification)
{
	// CVarSet::getSingleton().setMultiple(Array<const Char*, 2>{"Device", "2"});
	Bool bBenchmark, bWorkgraphs;
	commonInitWg(bBenchmark, bWorkgraphs);

	{
		const Char* kSrc = R"(
struct FirstNodeRecord
{
	uint3 m_dispatchGrid : SV_DispatchGrid;
};

struct SecondNodeRecord
{
	uint3 m_dispatchGrid : SV_DispatchGrid;
	uint m_objectIndex;
};

struct Aabb
{
	uint m_min;
	uint m_max;
};

struct Object
{
	uint m_positionsStart; // Points to g_positions
	uint m_positionCount;
};

RWStructuredBuffer<Aabb> g_aabbs : register(u0);
StructuredBuffer<Object> g_objects : register(t0);
StructuredBuffer<uint> g_positions : register(t1);

#define THREAD_COUNT 64u

// Operates per object
[Shader("node")] [NodeLaunch("broadcasting")] [NodeIsProgramEntry] [NodeMaxDispatchGrid(1, 1, 1)] [NumThreads(THREAD_COUNT, 1, 1)]
void main(DispatchNodeInputRecord<FirstNodeRecord> inp, [MaxRecords(THREAD_COUNT)] NodeOutput<SecondNodeRecord> computeAabb,
		  uint svGroupIndex : SV_GroupIndex, uint svDispatchThreadId : SV_DispatchThreadId)
{
	GroupNodeOutputRecords<SecondNodeRecord> recs = computeAabb.GetGroupNodeOutputRecords(THREAD_COUNT);

	const Object obj = g_objects[svDispatchThreadId];

	recs[svGroupIndex].m_objectIndex = svDispatchThreadId;
	recs[svGroupIndex].m_dispatchGrid = uint3((obj.m_positionCount + (THREAD_COUNT - 1)) / THREAD_COUNT, 1, 1);

	recs.OutputComplete();
}

groupshared Aabb g_aabb;

// Operates per position
[Shader("node")] [NodeLaunch("broadcasting")] [NodeMaxDispatchGrid(1, 1, 1)] [NumThreads(THREAD_COUNT, 1, 1)]
void computeAabb(DispatchNodeInputRecord<SecondNodeRecord> inp, uint svDispatchThreadId : SV_DispatchThreadId, uint svGroupIndex : SV_GroupIndex)
{
	const Object obj = g_objects[inp.Get().m_objectIndex];

	svDispatchThreadId = min(svDispatchThreadId, obj.m_positionCount - 1);

	if(svGroupIndex == 0)
	{
		g_aabb.m_min = 0xFFFFFFFF;
		g_aabb.m_max = 0;
	}

	Barrier(GROUP_SHARED_MEMORY, GROUP_SCOPE | GROUP_SYNC);

	const uint positionIndex = obj.m_positionsStart + svDispatchThreadId;

	const uint pos = g_positions[positionIndex];
	InterlockedMin(g_aabb.m_min, pos);
	InterlockedMax(g_aabb.m_max, pos);

	Barrier(GROUP_SHARED_MEMORY, GROUP_SCOPE | GROUP_SYNC);

	InterlockedMin(g_aabbs[inp.Get().m_objectIndex].m_min, g_aabb.m_min);
	InterlockedMax(g_aabbs[inp.Get().m_objectIndex].m_max, g_aabb.m_max);
}
)";

		const Char* kComputeSrc = R"(
struct Aabb
{
	uint m_min;
	uint m_max;
};

struct Object
{
	uint m_positionsStart; // Points to g_positions
	uint m_positionCount;
};

struct PushConsts
{
	uint m_objectIndex;
	uint m_padding1;
	uint m_padding2;
	uint m_padding3;
};

RWStructuredBuffer<Aabb> g_aabbs : register(u0);
StructuredBuffer<Object> g_objects : register(t0);
StructuredBuffer<uint> g_positions : register(t1);

#if defined(__spirv__)
[[vk::push_constant]] ConstantBuffer<PushConsts> g_consts;
#else
ConstantBuffer<PushConsts> g_consts : register(b0, space3000);
#endif

#define THREAD_COUNT 64u

groupshared Aabb g_aabb;

[NumThreads(THREAD_COUNT, 1, 1)]
void main(uint svDispatchThreadId : SV_DispatchThreadId, uint svGroupIndex : SV_GroupIndex)
{
	const Object obj = g_objects[g_consts.m_objectIndex];

	svDispatchThreadId = min(svDispatchThreadId, obj.m_positionCount - 1);

	if(svGroupIndex == 0)
	{
		g_aabb.m_min = 0xFFFFFFFF;
		g_aabb.m_max = 0;
	}

	Barrier(GROUP_SHARED_MEMORY, GROUP_SCOPE | GROUP_SYNC);

	const uint positionIndex = obj.m_positionsStart + svDispatchThreadId;

	const uint pos = g_positions[positionIndex];
	InterlockedMin(g_aabb.m_min, pos);
	InterlockedMax(g_aabb.m_max, pos);

	Barrier(GROUP_SHARED_MEMORY, GROUP_SCOPE | GROUP_SYNC);

	InterlockedMin(g_aabbs[g_consts.m_objectIndex].m_min, g_aabb.m_min);
	InterlockedMax(g_aabbs[g_consts.m_objectIndex].m_max, g_aabb.m_max);
}
)";

		constexpr U32 kObjectCount = 1000 * 64;
		constexpr U32 kPositionsPerObject = 10 * 64; // 1 * 1024;
		constexpr U32 kThreadCount = 64;

		ShaderProgramPtr prog;
		if(bWorkgraphs)
		{
			ShaderPtr shader = createShader(kSrc, ShaderType::kWorkGraph);
			ShaderProgramInitInfo progInit;
			Array<WorkGraphNodeSpecialization, 2> specializations = {
				{{"main", UVec3((kObjectCount + kThreadCount - 1) / kThreadCount, 1, 1)},
				 {"computeAabb", UVec3((kPositionsPerObject + (kThreadCount - 1)) / kThreadCount, 1, 1)}}};
			progInit.m_workGraph.m_nodeSpecializations = specializations;
			progInit.m_workGraph.m_shader = shader.get();
			prog = GrManager::getSingleton().newShaderProgram(progInit);
		}
		else
		{
			ShaderPtr shader = createShader(kComputeSrc, ShaderType::kCompute);
			ShaderProgramInitInfo progInit;
			progInit.m_computeShader = shader.get();
			prog = GrManager::getSingleton().newShaderProgram(progInit);
		}

		struct Aabb
		{
			U32 m_min = kMaxU32;
			U32 m_max = 0;

			Bool operator==(const Aabb&) const = default;
		};

		struct Object
		{
			U32 m_positionsStart; // Points to g_positions
			U32 m_positionCount;
		};

		// Objects
		DynamicArray<Object> objects;
		objects.resize(kObjectCount);
		U32 positionCount = 0;
		for(Object& obj : objects)
		{
			obj.m_positionsStart = positionCount;
			obj.m_positionCount = kPositionsPerObject;

			positionCount += obj.m_positionCount;
		}

		printf("Obj count %u, pos count %u\n", kObjectCount, positionCount);

		BufferPtr objBuff = createBuffer(BufferUsageBit::kSrvCompute, ConstWeakArray(objects), "Objects");

		// AABBs
		BufferPtr aabbsBuff = createBuffer(BufferUsageBit::kUavCompute, Aabb(), kObjectCount, "AABBs");

		// Positions
		GrDynamicArray<U32> positions;
		positions.resize(positionCount);
		positionCount = 0;
		for(U32 iobj = 0; iobj < kObjectCount; ++iobj)
		{
			const Object& obj = objects[iobj];

			const U32 min = getRandomRange<U32>(0, kMaxU32 / 2 - 1);
			const U32 max = getRandomRange<U32>(kMaxU32 / 2, kMaxU32);

			for(U32 ipos = obj.m_positionsStart; ipos < obj.m_positionsStart + obj.m_positionCount; ++ipos)
			{
				positions[ipos] = getRandomRange<U32>(min, max);

				positions[ipos] = iobj;
			}

			positionCount += obj.m_positionCount;
		}

		BufferPtr posBuff = createBuffer(BufferUsageBit::kSrvCompute, ConstWeakArray(positions), "Positions");

		BufferPtr scratchBuff;
		if(bWorkgraphs)
		{
			BufferInitInfo scratchInit("scratch");
			scratchInit.m_size = prog->getWorkGraphMemoryRequirements();
			scratchInit.m_usage = BufferUsageBit::kAllUav;
			scratchBuff = GrManager::getSingleton().newBuffer(scratchInit);
		}

		// Execute
		const U32 iterationsPerCmdb = (!bBenchmark) ? 1 : 100u;
		const U32 iterationCount = (!bBenchmark) ? iterationsPerCmdb : iterationsPerCmdb * 1;
		runBenchmark(iterationCount, iterationsPerCmdb, bBenchmark, [&](CommandBuffer& cmdb) {
			const BufferBarrierInfo barr = {BufferView(aabbsBuff.get()), BufferUsageBit::kUavCompute, BufferUsageBit::kUavCompute};
			cmdb.setPipelineBarrier({}, {&barr, 1}, {});

			if(bWorkgraphs)
			{
				struct FirstNodeRecord
				{
					UVec3 m_gridSize;
				};

				Array<FirstNodeRecord, 1> records;
				records[0].m_gridSize = UVec3((objects.getSize() + kThreadCount - 1) / kThreadCount, 1, 1);

				cmdb.bindShaderProgram(prog.get());
				cmdb.bindUav(0, 0, BufferView(aabbsBuff.get()));
				cmdb.bindSrv(0, 0, BufferView(objBuff.get()));
				cmdb.bindSrv(1, 0, BufferView(posBuff.get()));
				cmdb.dispatchGraph(BufferView(scratchBuff.get()), records.getBegin(), records.getSize(), sizeof(records[0]));
			}
			else
			{
				cmdb.bindShaderProgram(prog.get());
				cmdb.bindUav(0, 0, BufferView(aabbsBuff.get()));
				cmdb.bindSrv(0, 0, BufferView(objBuff.get()));
				cmdb.bindSrv(1, 0, BufferView(posBuff.get()));

				for(U32 iobj = 0; iobj < kObjectCount; ++iobj)
				{
					const UVec4 pc(iobj);
					cmdb.setFastConstants(&pc, sizeof(pc));

					cmdb.dispatchCompute((objects[iobj].m_positionCount + kThreadCount - 1) / kThreadCount, 1, 1);
				}
			}
		});

		// Check
		DynamicArray<Aabb> aabbs;
		readBuffer(aabbsBuff, aabbs);
		for(U32 i = 0; i < kObjectCount; ++i)
		{
			const Object& obj = objects[i];
			Aabb aabb;
			for(U32 ipos = obj.m_positionsStart; ipos < obj.m_positionsStart + obj.m_positionCount; ++ipos)
			{
				aabb.m_min = min(aabb.m_min, positions[ipos]);
				aabb.m_max = max(aabb.m_max, positions[ipos]);
			}

			if(aabb != aabbs[i])
			{
				printf("%u: %u %u | %u %u\n", i, aabb.m_min, aabbs[i].m_min, aabb.m_max, aabbs[i].m_max);
			}
			ANKI_TEST_EXPECT_EQ(aabb, aabbs[i]);
		}
	}

	commonDestroy();
}

ANKI_TEST(Gr, WorkGraphsWorkDrain)
{
	Bool bBenchmark, bWorkgraphs;
	commonInitWg(bBenchmark, bWorkgraphs);

#define TEX_SIZE_X 4096u
#define TEX_SIZE_Y 4096u
#define TILE_SIZE_X 32u
#define TILE_SIZE_Y 32u
#define TILE_COUNT_X (TEX_SIZE_X / TILE_SIZE_X)
#define TILE_COUNT_Y (TEX_SIZE_Y / TILE_SIZE_Y)
#define TILE_COUNT (TILE_COUNT_X * TILE_COUNT_Y)

	{
		// Create WG prog
		ShaderProgramPtr wgProg;
		if(bWorkgraphs)
		{
			ShaderPtr wgShader = loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/WorkDrainWg.hlsl", ShaderType::kWorkGraph);

			ShaderProgramInitInfo progInit;
			progInit.m_workGraph.m_shader = wgShader.get();
			Array<WorkGraphNodeSpecialization, 1> specializations = {{{"main", UVec3(TILE_COUNT_X, TILE_COUNT_Y, 1)}}};
			progInit.m_workGraph.m_nodeSpecializations = specializations;
			wgProg = GrManager::getSingleton().newShaderProgram(progInit);
		}

		// Scratch buff
		BufferPtr scratchBuff;
		if(bWorkgraphs)
		{
			BufferInitInfo scratchInit("scratch");
			scratchInit.m_size = wgProg->getWorkGraphMemoryRequirements();
			scratchInit.m_usage = BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv;
			scratchBuff = GrManager::getSingleton().newBuffer(scratchInit);
		}

		// Create compute progs
		ShaderProgramPtr compProg0, compProg1;
		{
			ShaderPtr shader =
				loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/WorkDrainCompute.hlsl", ShaderType::kCompute, Array<CString, 1>{"-DFIRST"});
			ShaderProgramInitInfo progInit;
			progInit.m_computeShader = shader.get();
			compProg0 = GrManager::getSingleton().newShaderProgram(progInit);

			shader = loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/WorkDrainCompute.hlsl", ShaderType::kCompute);
			progInit.m_computeShader = shader.get();
			compProg1 = GrManager::getSingleton().newShaderProgram(progInit);
		}

		// Create texture 2D
		TexturePtr tex;
		{
			DynamicArray<Vec4> data;
			data.resize(TEX_SIZE_X * TEX_SIZE_Y, Vec4(1.0f));
			data[10] = Vec4(1.1f, 2.06f, 3.88f, 0.5f);

			TextureInitInfo texInit("Tex");
			texInit.m_width = TEX_SIZE_X;
			texInit.m_height = TEX_SIZE_Y;
			texInit.m_format = Format::kR32G32B32A32_Sfloat;
			texInit.m_usage = TextureUsageBit::kAllUav | TextureUsageBit::kAllSrv;

			tex = createTexture2d(texInit, ConstWeakArray(data));
		}

		// Create counter buff
		BufferPtr threadgroupCountBuff = createBuffer(BufferUsageBit::kUavCompute, U32(0u), 1);

		// Result buffers
		BufferPtr tileMax = createBuffer(BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv, Vec4(0.1f), TILE_COUNT);
		BufferPtr finalMax = createBuffer(BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv, Vec4(0.1f), 1);

		const U32 iterationsPerCmdb = (!bBenchmark) ? 1 : 100u;
		const U32 iterationCount = (!bBenchmark) ? 1 : iterationsPerCmdb * 100;
		runBenchmark(iterationCount, iterationsPerCmdb, bBenchmark, [&](CommandBuffer& cmdb) {
			BufferBarrierInfo barr = {BufferView(tileMax.get()), BufferUsageBit::kUavCompute, BufferUsageBit::kUavCompute};
			cmdb.setPipelineBarrier({}, {&barr, 1}, {});

			cmdb.bindSrv(0, 0, TextureView(tex.get(), TextureSubresourceDesc::all()));
			cmdb.bindUav(0, 0, BufferView(tileMax.get()));
			cmdb.bindUav(1, 0, BufferView(finalMax.get()));
			cmdb.bindUav(2, 0, BufferView(threadgroupCountBuff.get()));

			if(bWorkgraphs)
			{
				cmdb.bindShaderProgram(wgProg.get());

				struct FirstNodeRecord
				{
					UVec3 m_gridSize;
				};

				Array<FirstNodeRecord, 1> records;
				records[0].m_gridSize = UVec3(TILE_COUNT_X, TILE_COUNT_Y, 1);

				cmdb.dispatchGraph(BufferView(scratchBuff.get()), records.getBegin(), records.getSize(), sizeof(records[0]));
			}
			else
			{
				cmdb.bindShaderProgram(compProg0.get());
				cmdb.dispatchCompute(TILE_COUNT_X, TILE_COUNT_Y, 1);

				barr = {BufferView(tileMax.get()), BufferUsageBit::kUavCompute, BufferUsageBit::kUavCompute};
				cmdb.setPipelineBarrier({}, {&barr, 1}, {});

				cmdb.bindShaderProgram(compProg1.get());
				cmdb.dispatchCompute(1, 1, 1);
			}
		});

		validateBuffer2(finalMax, Vec4(1.1f, 2.06f, 3.88f, 1.0f));
	}

	commonDestroy();
}

ANKI_TEST(Gr, WorkGraphsOverhead)
{
	Bool bBenchmark, bWorkgraphs;
	commonInitWg(bBenchmark, bWorkgraphs);

	constexpr U32 kMaxCandidates = 100 * 1024;

	{
		// Create compute progs
		ShaderProgramPtr compProg;
		{
			ShaderPtr shader =
				loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/FindPrimeNumbers.hlsl", ShaderType::kCompute, Array<CString, 1>{"-DWORKGRAPHS=0"});
			ShaderProgramInitInfo progInit;
			progInit.m_computeShader = shader.get();
			compProg = GrManager::getSingleton().newShaderProgram(progInit);
		}

		// Create WG prog
		ShaderProgramPtr wgProg;
		if(bWorkgraphs)
		{
			ShaderPtr shader =
				loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/FindPrimeNumbers.hlsl", ShaderType::kWorkGraph, Array<CString, 1>{"-DWORKGRAPHS=1"});

			ShaderProgramInitInfo progInit;
			progInit.m_workGraph.m_shader = shader.get();
			Array<WorkGraphNodeSpecialization, 1> specializations = {{{"main", UVec3(kMaxCandidates, 1, 1)}}};
			progInit.m_workGraph.m_nodeSpecializations = specializations;
			wgProg = GrManager::getSingleton().newShaderProgram(progInit);
		}

		// Scratch buff
		BufferPtr scratchBuff;
		if(bWorkgraphs)
		{
			BufferInitInfo scratchInit("scratch");
			scratchInit.m_size = wgProg->getWorkGraphMemoryRequirements();
			scratchInit.m_usage = BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv;
			scratchBuff = GrManager::getSingleton().newBuffer(scratchInit);
		}

		BufferPtr miscBuff = createBuffer(BufferUsageBit::kUavCompute, UVec3(0, kMaxCandidates, 0), 2);

		BufferPtr primeNumbersBuff = createBuffer(BufferUsageBit::kUavCompute, U32(0u), kMaxCandidates + 1);

		const U32 iterationsPerCmdb = (!bBenchmark) ? 1 : 100u;
		const U32 iterationCount = (!bBenchmark) ? 1 : iterationsPerCmdb * 50;
		runBenchmark(iterationCount, iterationsPerCmdb, bBenchmark, [&](CommandBuffer& cmdb) {
			BufferBarrierInfo barr = {BufferView(primeNumbersBuff.get()), BufferUsageBit::kUavCompute, BufferUsageBit::kUavCompute};
			cmdb.setPipelineBarrier({}, {&barr, 1}, {});

			cmdb.bindUav(0, 0, BufferView(primeNumbersBuff.get()));
			cmdb.bindUav(1, 0, BufferView(miscBuff.get()));

			if(bWorkgraphs)
			{
				cmdb.bindShaderProgram(wgProg.get());

				struct FirstNodeRecord
				{
					UVec3 m_gridSize;
				};

				Array<FirstNodeRecord, 1> records;
				records[0].m_gridSize = UVec3(kMaxCandidates, 1, 1);

				cmdb.dispatchGraph(BufferView(scratchBuff.get()), records.getBegin(), records.getSize(), sizeof(records[0]));
			}
			else
			{
				cmdb.bindShaderProgram(compProg.get());

				cmdb.dispatchCompute(kMaxCandidates, 1, 1);
			}
		});

		DynamicArray<U32> values;
		readBuffer(primeNumbersBuff, values);

		values.resize(values[0] + 1);

		std::sort(values.getBegin() + 1, values.getEnd(), [](U32 a, U32 b) {
			return a < b;
		});

		auto isPrime = [](int N) {
			if(N <= 1)
			{
				return false;
			}

			for(int i = 2; i < N / 2; i++)
			{
				if(N % i == 0)
				{
					return false;
				}
			}

			return true;
		};

		DynamicArray<U32> values2;
		values2.resize(1, 0);
		for(U32 i = 0; i < kMaxCandidates; ++i)
		{
			if(isPrime(i))
			{
				++values2[0];
				values2.emplaceBack(i);
			}
		}

		ANKI_TEST_EXPECT_EQ(values.getSize(), values2.getSize());
		for(U32 i = 0; i < values2.getSize(); ++i)
		{
			ANKI_TEST_EXPECT_EQ(values[i], values2[i]);
		}
	}

	commonDestroy();
}

ANKI_TEST(Gr, WorkGraphsJobManager)
{
	Bool bBenchmark, bWorkgraphs;
	// CVarSet::getSingleton().setMultiple(Array<const Char*, 2>{"Device", "1"});
	commonInitWg(bBenchmark, bWorkgraphs);

	const U32 queueRingBufferSize = nextPowerOfTwo(2 * 1024 * 1024);
	const U32 initialWorkItemCount = 128 * 1024;

	{
		// Create compute progs
		ShaderProgramPtr compProg;
		{
			ShaderPtr shader = loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/JobManagerCompute.hlsl", ShaderType::kCompute);
			ShaderProgramInitInfo progInit;
			progInit.m_computeShader = shader.get();
			compProg = GrManager::getSingleton().newShaderProgram(progInit);
		}

		ShaderProgramPtr wgProg;
		if(bWorkgraphs)
		{
			ShaderPtr shader = loadShader(ANKI_SOURCE_DIRECTORY "/Tests/Gr/JobManagerWg.hlsl", ShaderType::kWorkGraph);

			ShaderProgramInitInfo progInit;
			Array<WorkGraphNodeSpecialization, 1> specializations = {{{"main", UVec3((initialWorkItemCount + 64 - 1) / 64, 1, 1)}}};
			progInit.m_workGraph.m_nodeSpecializations = specializations;
			progInit.m_workGraph.m_shader = shader.get();
			wgProg = GrManager::getSingleton().newShaderProgram(progInit);
		}

		// Scratch buff
		BufferPtr scratchBuff;
		if(bWorkgraphs)
		{
			BufferInitInfo scratchInit("scratch");
			scratchInit.m_size = wgProg->getWorkGraphMemoryRequirements();
			scratchInit.m_usage = BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv;
			scratchBuff = GrManager::getSingleton().newBuffer(scratchInit);
		}

		DynamicArray<U32> initialWorkItems;
		U32 finalValue = 0;
		U32 workItemCount = 0;
		{
			initialWorkItems.resize(initialWorkItemCount);
			for(U32 i = 0; i < initialWorkItems.getSize(); ++i)
			{
				const Bool bDeterministic = bBenchmark;
				const U32 level = ((bDeterministic) ? i : rand()) % 4;
				const U32 payload = ((bDeterministic) ? 1 : rand()) % 4;

				initialWorkItems[i] = (level << 16) | payload;
			}

			DynamicArray<U32> initialWorkItems2 = initialWorkItems;
			while(initialWorkItems2.getSize() > 0)
			{
				const U32 workItem = initialWorkItems2.getBack();
				initialWorkItems2.popBack();
				const U32 level = workItem >> 16u;
				const U32 payload = workItem & 0xFFFFu;

				++workItemCount;

				if(level == 0)
				{
					finalValue += payload;
				}
				else
				{
					U32 newWorkItem = (level - 1) << 16u;
					newWorkItem |= payload;

					for(U32 i = 0; i < 4; ++i)
					{
						initialWorkItems2.emplaceBack(newWorkItem);
					}
				}
			};
		}

		BufferPtr resultBuff = createBuffer<U32>(BufferUsageBit::kAllUav, 0u, 2);

		BufferPtr queueRingBuff;
		if(!bWorkgraphs)
		{
			queueRingBuff = createBuffer<U32>(BufferUsageBit::kAllUav, 0u, queueRingBufferSize);

			BufferPtr tempBuff = createBuffer<U32>(BufferUsageBit::kCopySource, initialWorkItems);

			CommandBufferPtr cmdb = GrManager::getSingleton().newCommandBuffer(CommandBufferInitInfo());
			cmdb->copyBufferToBuffer(BufferView(tempBuff.get(), 0, initialWorkItems.getSizeInBytes()),
									 BufferView(queueRingBuff.get(), 0, initialWorkItems.getSizeInBytes()));
			cmdb->endRecording();

			FencePtr fence;
			GrManager::getSingleton().submit(cmdb.get(), {}, &fence);

			ANKI_TEST_EXPECT_EQ(fence->clientWait(kMaxSecond), true);
		}

		BufferPtr initialWorkItemsBuff;
		if(bWorkgraphs)
		{
			initialWorkItemsBuff = createBuffer<U32>(BufferUsageBit::kAllUav, 0u, initialWorkItemCount);

			BufferPtr tempBuff = createBuffer<U32>(BufferUsageBit::kCopySource, initialWorkItems);

			CommandBufferPtr cmdb = GrManager::getSingleton().newCommandBuffer(CommandBufferInitInfo());
			cmdb->copyBufferToBuffer(BufferView(tempBuff.get(), 0, initialWorkItems.getSizeInBytes()),
									 BufferView(initialWorkItemsBuff.get(), 0, initialWorkItems.getSizeInBytes()));
			cmdb->endRecording();

			FencePtr fence;
			GrManager::getSingleton().submit(cmdb.get(), {}, &fence);

			ANKI_TEST_EXPECT_EQ(fence->clientWait(kMaxSecond), true);
		}

		BufferPtr queueBuff;
		if(!bWorkgraphs)
		{
			struct Queue
			{
				U32 m_spinlock;
				U32 m_head;
				U32 m_tail;
				U32 m_pendingWork;
			};

			Queue q = {};
			q.m_head = initialWorkItems.getSize();

			queueBuff = createBuffer(BufferUsageBit::kAllUav, q, 1);
		}

		ANKI_TEST_LOGI("Init complete");

		const U32 iterationsPerCmdb = 1;
		const U32 iterationCount = 1;
		runBenchmark(iterationCount, iterationsPerCmdb, bBenchmark, [&](CommandBuffer& cmdb) {
			if(!bWorkgraphs)
			{
				cmdb.bindShaderProgram(compProg.get());

				cmdb.bindUav(0, 0, BufferView(queueBuff.get()));
				cmdb.bindUav(1, 0, BufferView(queueRingBuff.get()));
				cmdb.bindUav(2, 0, BufferView(resultBuff.get()));
				UVec4 consts(queueRingBufferSize - 1);
				cmdb.setFastConstants(&consts, sizeof(consts));

				cmdb.dispatchCompute(256, 1, 1);
			}
			else
			{
				cmdb.bindShaderProgram(wgProg.get());

				cmdb.bindSrv(0, 0, BufferView(initialWorkItemsBuff.get()));
				cmdb.bindUav(0, 0, BufferView(resultBuff.get()));

				struct FirstNodeRecord
				{
					UVec3 m_gridSize;
				};

				Array<FirstNodeRecord, 1> records;
				records[0].m_gridSize = UVec3((initialWorkItemCount + 64 - 1) / 64, 1, 1);

				cmdb.dispatchGraph(BufferView(scratchBuff.get()), records.getBegin(), records.getSize(), sizeof(records[0]));
			}
		});

		DynamicArray<U32> result;
		readBuffer(resultBuff, result);
		printf("expecting %u, got %u. Error %u\n", finalValue, result[0], result[1]);
		ANKI_TEST_EXPECT_EQ(result[0], finalValue);
		ANKI_TEST_EXPECT_EQ(result[1], 0);
	}

	commonDestroy();
}
