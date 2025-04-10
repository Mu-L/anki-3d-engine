// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/ShaderCompiler/RadeonGpuAnalyzer.h>
#include <AnKi/Util/File.h>
#include <AnKi/Util/Filesystem.h>
#include <AnKi/Util/Process.h>
#include <AnKi/Util/StringList.h>

namespace anki {

static Atomic<U32> g_nextFileId = {1};

static CString getPipelineStageString(ShaderType shaderType)
{
	CString out;
	switch(shaderType)
	{
	case ShaderType::kVertex:
		out = "vert";
		break;
	case ShaderType::kPixel:
		out = "frag";
		break;
	case ShaderType::kCompute:
		out = "comp";
		break;
	default:
		ANKI_ASSERT(!"TODO");
	}

	return out;
}

Error runRadeonGpuAnalyzer(ConstWeakArray<U8> spirv, ShaderType shaderType, RgaOutput& out)
{
	ANKI_ASSERT(spirv.getSize() > 0);
	const U32 rand = g_nextFileId.fetchAdd(1) + getCurrentProcessId();

	// Store SPIRV
	String tmpDir;
	ANKI_CHECK(getTempDirectory(tmpDir));
	ShaderCompilerString spvFilename;
	spvFilename.sprintf("%s/AnKiRgaInput_%u.spv", tmpDir.cstr(), rand);
	File spvFile;
	ANKI_CHECK(spvFile.open(spvFilename, FileOpenFlag::kWrite | FileOpenFlag::kBinary));
	ANKI_CHECK(spvFile.write(&spirv[0], spirv.getSizeInBytes()));
	spvFile.close();
	CleanupFile spvFileCleanup(spvFilename);

	// Call RGA
	ShaderCompilerString analysisFilename;
	analysisFilename.sprintf("%s/AnKiRgaOutAnalysis_%u.csv", tmpDir.cstr(), rand);

	ShaderCompilerString stageStr = "--";
	stageStr += getPipelineStageString(shaderType);

	Array<CString, 8> args;
	args[0] = "-s";
	args[1] = "vk-spv-offline";
	args[2] = "-c";
	args[3] = "gfx1030"; // Target RDNA2
	args[4] = "-a";
	args[5] = analysisFilename;
	args[6] = stageStr;
	args[7] = spvFilename;

	I32 exitCode;
#if ANKI_OS_LINUX
	CString rgaExecutable = ANKI_SOURCE_DIRECTORY "/ThirdParty/Bin/Linux64/RadeonGpuAnalyzer/rga";
#elif ANKI_OS_WINDOWS
	CString rgaExecutable = ANKI_SOURCE_DIRECTORY "/ThirdParty/Bin/Windows64/RadeonGpuAnalyzer/rga.exe";
#else
	CString rgaExecutable = "nothing";
	ANKI_ASSERT(0);
#endif
	ShaderCompilerString argsStr;
	for(CString a : args)
	{
		argsStr += a;
		argsStr += " ";
	}
	ANKI_SHADER_COMPILER_LOGV("Calling RGA: %s %s", rgaExecutable.cstr(), argsStr.cstr());

	ANKI_CHECK(Process::callProcess(rgaExecutable, args, nullptr, nullptr, exitCode));

	if(exitCode != 0)
	{
		ANKI_SHADER_COMPILER_LOGE("RGA failed with exit code %d", exitCode);
		return Error::kFunctionFailed;
	}

	// Construct the output filename
	ShaderCompilerString outFilename;
	outFilename.sprintf("%s/gfx1030_AnKiRgaOutAnalysis_%u_%s.csv", tmpDir.cstr(), rand, getPipelineStageString(shaderType).cstr());

	CleanupFile rgaFileCleanup(outFilename);

	// Read the file
	File analysisFile;
	ANKI_CHECK(analysisFile.open(outFilename, FileOpenFlag::kRead));
	ShaderCompilerString analysisText;
	ANKI_CHECK(analysisFile.readAllText(analysisText));
	analysisText.replaceAll("\r", "");
	analysisFile.close();

	// Parse the text
	ShaderCompilerStringList lines;
	lines.splitString(analysisText, '\n');
	if(lines.getSize() != 2)
	{
		ANKI_SHADER_COMPILER_LOGE("Error parsing RGA analysis file");
		return Error::kFunctionFailed;
	}

	ShaderCompilerStringList tokens;
	tokens.splitString(lines.getFront(), ',');

	ShaderCompilerStringList values;
	values.splitString(*(lines.getBegin() + 1), ',');

	if(tokens.getSize() != tokens.getSize())
	{
		ANKI_SHADER_COMPILER_LOGE("Error parsing RGA analysis file");
		return Error::kFunctionFailed;
	}

	out.m_vgprCount = kMaxU32;
	out.m_sgprCount = kMaxU32;
	out.m_isaSize = kMaxU32;
	auto valuesIt = values.getBegin();
	for(const ShaderCompilerString& token : tokens)
	{
		if(token.find("USED_VGPRs") != String::kNpos)
		{
			ANKI_CHECK((*valuesIt).toNumber(out.m_vgprCount));
		}
		else if(token.find("USED_SGPRs") != String::kNpos)
		{
			ANKI_CHECK((*valuesIt).toNumber(out.m_sgprCount));
		}
		else if(token.find("ISA_SIZE") != String::kNpos)
		{
			ANKI_CHECK((*valuesIt).toNumber(out.m_isaSize));
		}

		++valuesIt;
	}

	if(out.m_vgprCount == kMaxU32 || out.m_sgprCount == kMaxU32 || out.m_isaSize == kMaxU32)
	{
		ANKI_SHADER_COMPILER_LOGE("Error parsing RGA analysis file");
		return Error::kFunctionFailed;
	}

	return Error::kNone;
}

} // end namespace anki
