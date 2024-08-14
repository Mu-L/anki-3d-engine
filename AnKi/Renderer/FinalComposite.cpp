// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/FinalComposite.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/Bloom.h>
#include <AnKi/Renderer/Scale.h>
#include <AnKi/Renderer/Tonemapping.h>
#include <AnKi/Renderer/LightShading.h>
#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/Dbg.h>
#include <AnKi/Renderer/DownscaleBlur.h>
#include <AnKi/Renderer/UiStage.h>
#include <AnKi/Renderer/MotionVectors.h>
#include <AnKi/Util/Logger.h>
#include <AnKi/Util/Tracer.h>
#include <AnKi/Core/CVarSet.h>

namespace anki {

static NumericCVar<U32> g_motionBlurSamplesCVar(CVarSubsystem::kRenderer, "MotionBlurSamples", 32, 0, 2048, "Max motion blur samples");
static NumericCVar<F32> g_filmGrainStrengthCVar(CVarSubsystem::kRenderer, "FilmGrainStrength", 16.0f, 0.0f, 250.0f, "Film grain strength");

Error FinalComposite::initInternal()
{
	ANKI_R_LOGV("Initializing final composite");

	ANKI_CHECK(loadColorGradingTextureImage("EngineAssets/DefaultLut.ankitex"));

	// Progs
	for(MutatorValue dbg = 0; dbg < 2; ++dbg)
	{
		ANKI_CHECK(loadShaderProgram("ShaderBinaries/FinalComposite.ankiprogbin",
									 {{"FILM_GRAIN", (g_filmGrainStrengthCVar.get() > 0.0) ? 1 : 0}, {"BLOOM_ENABLED", 1}, {"DBG_ENABLED", dbg}},
									 m_prog, m_grProgs[dbg]));
	}

	ANKI_CHECK(loadShaderProgram("ShaderBinaries/VisualizeRenderTarget.ankiprogbin", m_defaultVisualizeRenderTargetProg,
								 m_defaultVisualizeRenderTargetGrProg));

	return Error::kNone;
}

Error FinalComposite::init()
{
	const Error err = initInternal();
	if(err)
	{
		ANKI_R_LOGE("Failed to init final composite");
	}

	return err;
}

Error FinalComposite::loadColorGradingTextureImage(CString filename)
{
	m_lut.reset(nullptr);
	ANKI_CHECK(ResourceManager::getSingleton().loadResource(filename, m_lut));
	ANKI_ASSERT(m_lut->getWidth() == m_lut->getHeight());
	ANKI_ASSERT(m_lut->getWidth() == m_lut->getDepth());

	return Error::kNone;
}

void FinalComposite::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(RFinalComposite);

	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	// Create the pass
	GraphicsRenderPass& pass = rgraph.newGraphicsRenderPass("Final Composite");

	pass.setWork([this](RenderPassWorkContext& rgraphCtx) {
		run(rgraphCtx);
	});
	pass.setRenderpassInfo({GraphicsRenderPassTargetDesc(ctx.m_outRenderTarget)});

	pass.newTextureDependency(ctx.m_outRenderTarget, TextureUsageBit::kRtvDsvWrite);

	if(g_dbgCVar.get())
	{
		pass.newTextureDependency(getRenderer().getDbg().getRt(), TextureUsageBit::kSrvFragment);
	}

	pass.newTextureDependency(getRenderer().getScale().getTonemappedRt(), TextureUsageBit::kSrvFragment);
	pass.newTextureDependency(getRenderer().getBloom().getRt(), TextureUsageBit::kSrvFragment);
	pass.newTextureDependency(getRenderer().getMotionVectors().getMotionVectorsRt(), TextureUsageBit::kSrvFragment);
	pass.newTextureDependency(getRenderer().getGBuffer().getDepthRt(), TextureUsageBit::kSrvFragment);

	Array<RenderTargetHandle, kMaxDebugRenderTargets> dbgRts;
	ShaderProgramPtr debugProgram;
	const Bool hasDebugRt = getRenderer().getCurrentDebugRenderTarget(dbgRts, debugProgram);
	if(hasDebugRt)
	{
		for(const RenderTargetHandle& handle : dbgRts)
		{
			if(handle.isValid())
			{
				pass.newTextureDependency(handle, TextureUsageBit::kSrvFragment);
			}
		}
	}
}

void FinalComposite::run(RenderPassWorkContext& rgraphCtx)
{
	ANKI_TRACE_SCOPED_EVENT(FinalComposite);

	CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;
	const Bool dbgEnabled = g_dbgCVar.get();

	Array<RenderTargetHandle, kMaxDebugRenderTargets> dbgRts;
	ShaderProgramPtr optionalDebugProgram;
	const Bool hasDebugRt = getRenderer().getCurrentDebugRenderTarget(dbgRts, optionalDebugProgram);

	// Bind program
	if(hasDebugRt && optionalDebugProgram.isCreated())
	{
		cmdb.bindShaderProgram(optionalDebugProgram.get());
	}
	else if(hasDebugRt)
	{
		cmdb.bindShaderProgram(m_defaultVisualizeRenderTargetGrProg.get());
	}
	else
	{
		cmdb.bindShaderProgram(m_grProgs[dbgEnabled].get());
	}

	// Bind stuff
	if(!hasDebugRt)
	{
		cmdb.bindSampler(0, 0, getRenderer().getSamplers().m_nearestNearestClamp.get());
		cmdb.bindSampler(1, 0, getRenderer().getSamplers().m_trilinearClamp.get());
		cmdb.bindSampler(2, 0, getRenderer().getSamplers().m_trilinearRepeat.get());

		rgraphCtx.bindSrv(0, 0, getRenderer().getScale().getTonemappedRt());

		rgraphCtx.bindSrv(1, 0, getRenderer().getBloom().getRt());
		cmdb.bindSrv(2, 0, TextureView(&m_lut->getTexture(), TextureSubresourceDesc::all()));
		rgraphCtx.bindSrv(3, 0, getRenderer().getMotionVectors().getMotionVectorsRt());
		rgraphCtx.bindSrv(4, 0, getRenderer().getGBuffer().getDepthRt());

		if(dbgEnabled)
		{
			rgraphCtx.bindSrv(5, 0, getRenderer().getDbg().getRt());
		}

		const UVec4 pc(g_motionBlurSamplesCVar.get(), floatBitsToUint(g_filmGrainStrengthCVar.get()), getRenderer().getFrameCount() & kMaxU32, 0);
		cmdb.setPushConstants(&pc, sizeof(pc));
	}
	else
	{
		cmdb.bindSampler(0, 0, getRenderer().getSamplers().m_nearestNearestClamp.get());

		U32 count = 0;
		for(const RenderTargetHandle& handle : dbgRts)
		{
			if(handle.isValid())
			{
				rgraphCtx.bindSrv(count++, 0, handle);
			}
		}
	}

	cmdb.setViewport(0, 0, getRenderer().getPostProcessResolution().x(), getRenderer().getPostProcessResolution().y());
	drawQuad(cmdb);

	// Draw UI
	getRenderer().getUiStage().draw(getRenderer().getPostProcessResolution().x(), getRenderer().getPostProcessResolution().y(), cmdb);
}

} // end namespace anki
