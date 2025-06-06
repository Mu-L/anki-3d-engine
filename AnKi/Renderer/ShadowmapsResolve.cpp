// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/ShadowmapsResolve.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/ShadowMapping.h>
#include <AnKi/Renderer/DepthDownscale.h>
#include <AnKi/Renderer/ClusterBinning.h>
#include <AnKi/Renderer/RtShadows.h>
#include <AnKi/Util/CVarSet.h>
#include <AnKi/Util/Tracer.h>

namespace anki {

Error ShadowmapsResolve::init()
{
	m_quarterRez = g_smResolveQuarterRezCVar;
	const U32 width = getRenderer().getInternalResolution().x() / (m_quarterRez + 1);
	const U32 height = getRenderer().getInternalResolution().y() / (m_quarterRez + 1);

	m_rtDescr = getRenderer().create2DRenderTargetDescription(width, height, Format::kR8G8B8A8_Unorm, "SM resolve");
	m_rtDescr.bake();

	// Prog
	for(MutatorValue quality = 0; quality < 3; ++quality)
	{
		ANKI_CHECK(loadShaderProgram("ShaderBinaries/ShadowmapsResolve.ankiprogbin",
									 {{"QUALITY", quality}, {"DIRECTIONAL_LIGHT_SHADOW_RESOLVED", isRtShadowsEnabled()}}, m_prog,
									 m_grProgs[quality]));
	}

	ANKI_CHECK(ResourceManager::getSingleton().loadResource("EngineAssets/BlueNoise_Rgba8_64x64.png", m_noiseImage));

	return Error::kNone;
}

void ShadowmapsResolve::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(ShadowmapsResolve);

	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;
	m_runCtx.m_rt = rgraph.newRenderTarget(m_rtDescr);

	if(g_preferComputeCVar)
	{
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("ResolveShadows");

		rpass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
			run(rgraphCtx, ctx);
		});

		rpass.newTextureDependency(m_runCtx.m_rt, TextureUsageBit::kUavCompute);
		rpass.newTextureDependency((m_quarterRez) ? getDepthDownscale().getRt() : getGBuffer().getDepthRt(), TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(getShadowMapping().getShadowmapRt(), TextureUsageBit::kSrvCompute);

		rpass.newBufferDependency(getClusterBinning().getDependency(), BufferUsageBit::kSrvCompute);

		if(isRtShadowsEnabled())
		{
			rpass.newTextureDependency(getRtShadows().getRt(), TextureUsageBit::kSrvCompute);
		}
	}
	else
	{
		GraphicsRenderPass& rpass = rgraph.newGraphicsRenderPass("ResolveShadows");

		rpass.setRenderpassInfo({GraphicsRenderPassTargetDesc(m_runCtx.m_rt)});

		rpass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
			run(rgraphCtx, ctx);
		});

		rpass.newTextureDependency(m_runCtx.m_rt, TextureUsageBit::kRtvDsvWrite);
		rpass.newTextureDependency((m_quarterRez) ? getDepthDownscale().getRt() : getGBuffer().getDepthRt(), TextureUsageBit::kSrvPixel);
		rpass.newTextureDependency(getShadowMapping().getShadowmapRt(), TextureUsageBit::kSrvPixel);

		rpass.newBufferDependency(getClusterBinning().getDependency(), BufferUsageBit::kSrvPixel);

		if(isRtShadowsEnabled())
		{
			rpass.newTextureDependency(getRtShadows().getRt(), TextureUsageBit::kSrvPixel);
		}
	}
}

void ShadowmapsResolve::run(RenderPassWorkContext& rgraphCtx, RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(ShadowmapsResolve);
	CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

	U32 quality;
	if(g_shadowMappingPcssCVar)
	{
		quality = 2;
	}
	else if(g_shadowMappingPcfCVar)
	{
		quality = 1;
	}
	else
	{
		quality = 0;
	}
	cmdb.bindShaderProgram(m_grProgs[quality].get());

	cmdb.bindConstantBuffer(0, 0, ctx.m_globalRenderingConstantsBuffer);
	cmdb.bindSrv(0, 0, getClusterBinning().getPackedObjectsBuffer(GpuSceneNonRenderableObjectType::kLight));
	cmdb.bindSrv(1, 0, getClusterBinning().getPackedObjectsBuffer(GpuSceneNonRenderableObjectType::kLight));
	rgraphCtx.bindSrv(2, 0, getShadowMapping().getShadowmapRt());
	cmdb.bindSrv(3, 0, getClusterBinning().getClustersBuffer());

	cmdb.bindSampler(0, 0, getRenderer().getSamplers().m_trilinearClamp.get());
	cmdb.bindSampler(1, 0, getRenderer().getSamplers().m_trilinearClampShadow.get());
	cmdb.bindSampler(2, 0, getRenderer().getSamplers().m_trilinearRepeat.get());

	if(m_quarterRez)
	{
		rgraphCtx.bindSrv(4, 0, getDepthDownscale().getRt(), DepthDownscale::kQuarterInternalResolution);
	}
	else
	{
		rgraphCtx.bindSrv(4, 0, getGBuffer().getDepthRt());
	}
	cmdb.bindSrv(5, 0, TextureView(&m_noiseImage->getTexture(), TextureSubresourceDesc::all()));

	if(isRtShadowsEnabled())
	{
		rgraphCtx.bindSrv(6, 0, getRtShadows().getRt());
	}

	if(g_preferComputeCVar || g_shadowMappingPcfCVar || g_shadowMappingPcssCVar)
	{
		const Vec4 consts(F32(m_rtDescr.m_width), F32(m_rtDescr.m_height), 0.0f, 0.0f);
		cmdb.setFastConstants(&consts, sizeof(consts));
	}

	if(g_preferComputeCVar)
	{
		rgraphCtx.bindUav(0, 0, m_runCtx.m_rt);
		dispatchPPCompute(cmdb, 8, 8, m_rtDescr.m_width, m_rtDescr.m_height);
	}
	else
	{
		cmdb.setViewport(0, 0, m_rtDescr.m_width, m_rtDescr.m_height);
		cmdb.draw(PrimitiveTopology::kTriangles, 3);
	}
}

} // end namespace anki
