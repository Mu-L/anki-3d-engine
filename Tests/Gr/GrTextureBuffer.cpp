// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <Tests/Framework/Framework.h>
#include <Tests/Gr/GrCommon.h>
#include <AnKi/Gr.h>

ANKI_TEST(Gr, TextureBuffer)
{
	g_validationCVar = true;

	initWindow();
	initGrManager();

	{
		const CString shaderSrc = R"(
layout(binding = 0) uniform textureBuffer u_tbuff;
layout(binding = 1) buffer b_buff
{
	Vec4 u_buff[];
};

void main()
{
	u_buff[0] = texelFetch(u_tbuff, I32(gl_GlobalInvocationID.x));
}
	)";

		ShaderPtr shader = createShader(shaderSrc, ShaderType::kCompute);

		ShaderProgramInitInfo progInit;
		progInit.m_computeShader = shader.get();
		ShaderProgramPtr prog = GrManager::getSingleton().newShaderProgram(progInit);

		BufferInitInfo buffInit;
		buffInit.m_mapAccess = BufferMapAccessBit::kWrite;
		buffInit.m_size = sizeof(U8) * 4;
		buffInit.m_usage = BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv;
		BufferPtr texBuff = GrManager::getSingleton().newBuffer(buffInit);

		I8* data = static_cast<I8*>(texBuff->map(0, kMaxPtrSize, BufferMapAccessBit::kWrite));
		const Vec4 values(-1.0f, -0.25f, 0.1345f, 0.8952f);
		for(U i = 0; i < 4; ++i)
		{
			data[i] = I8(values[i] * 127.0f);
		}

		texBuff->unmap();

		buffInit.m_mapAccess = BufferMapAccessBit::kRead;
		buffInit.m_size = sizeof(F32) * 4;
		buffInit.m_usage = BufferUsageBit::kAllUav | BufferUsageBit::kAllSrv;
		BufferPtr storageBuff = GrManager::getSingleton().newBuffer(buffInit);

		CommandBufferInitInfo cmdbInit;
		cmdbInit.m_flags = CommandBufferFlag::kSmallBatch | CommandBufferFlag::kGeneralWork;
		CommandBufferPtr cmdb = GrManager::getSingleton().newCommandBuffer(cmdbInit);

		cmdb->bindSrv(0, 0, BufferView(texBuff.get()), Format::kR8G8B8A8_Snorm);
		cmdb->bindUav(0, 0, BufferView(storageBuff.get()));
		cmdb->bindShaderProgram(prog.get());
		cmdb->dispatchCompute(1, 1, 1);
		cmdb->endRecording();
		GrManager::getSingleton().submit(cmdb.get());
		GrManager::getSingleton().finish();

		const Vec4* inData = static_cast<const Vec4*>(storageBuff->map(0, kMaxPtrSize, BufferMapAccessBit::kRead));
		for(U i = 0; i < 4; ++i)
		{
			ANKI_TEST_EXPECT_NEAR(values[i], (*inData)[i], 0.01f);
		}

		storageBuff->unmap();
	}

	GrManager::freeSingleton();
	NativeWindow::freeSingleton();
}
