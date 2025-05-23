// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/VolumetricFog.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/DepthDownscale.h>
#include <AnKi/Renderer/ShadowMapping.h>
#include <AnKi/Renderer/LightShading.h>
#include <AnKi/Renderer/VolumetricLightingAccumulation.h>
#include <AnKi/Util/CVarSet.h>
#include <AnKi/Scene/Components/SkyboxComponent.h>
#include <AnKi/Util/Tracer.h>

namespace anki {

Error VolumetricFog::init()
{
	// Misc
	const F32 qualityXY = g_volumetricLightingAccumulationQualityXYCVar;
	const F32 qualityZ = g_volumetricLightingAccumulationQualityZCVar;
	m_finalZSplit = min<U32>(getRenderer().getZSplitCount() - 1, g_volumetricLightingAccumulationFinalZSplitCVar);

	m_volumeSize[0] = U32(F32(getRenderer().getTileCounts().x()) * qualityXY);
	m_volumeSize[1] = U32(F32(getRenderer().getTileCounts().y()) * qualityXY);
	m_volumeSize[2] = U32(F32(m_finalZSplit + 1) * qualityZ);

	// Shaders
	ANKI_CHECK(loadShaderProgram("ShaderBinaries/VolumetricFogAccumulation.ankiprogbin", m_prog, m_grProg));

	// RT descr
	m_rtDescr = getRenderer().create2DRenderTargetDescription(m_volumeSize[0], m_volumeSize[1], Format::kR16G16B16A16_Sfloat, "Fog");
	m_rtDescr.m_depth = m_volumeSize[2];
	m_rtDescr.m_type = TextureType::k3D;
	m_rtDescr.bake();

	return Error::kNone;
}

void VolumetricFog::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(VolumetricFog);
	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	m_runCtx.m_rt = rgraph.newRenderTarget(m_rtDescr);

	NonGraphicsRenderPass& pass = rgraph.newNonGraphicsRenderPass("Vol fog");

	pass.newTextureDependency(m_runCtx.m_rt, TextureUsageBit::kUavCompute);
	pass.newTextureDependency(getRenderer().getVolumetricLightingAccumulation().getRt(), TextureUsageBit::kSrvCompute);

	pass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
		ANKI_TRACE_SCOPED_EVENT(VolumetricFog);
		CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

		cmdb.bindShaderProgram(m_grProg.get());

		cmdb.bindSampler(0, 0, getRenderer().getSamplers().m_trilinearClamp.get());
		rgraphCtx.bindSrv(0, 0, getRenderer().getVolumetricLightingAccumulation().getRt());

		rgraphCtx.bindUav(0, 0, m_runCtx.m_rt);

		const SkyboxComponent* sky = SceneGraph::getSingleton().getSkybox();

		VolumetricFogConstants consts;
		consts.m_fogDiffuse = (sky) ? sky->getFogDiffuseColor() : Vec3(0.0f);
		consts.m_fogScatteringCoeff = (sky) ? sky->getFogScatteringCoefficient() : 0.0f;
		consts.m_fogAbsorptionCoeff = (sky) ? sky->getFogAbsorptionCoefficient() : 0.0f;
		consts.m_near = ctx.m_matrices.m_near;
		consts.m_far = ctx.m_matrices.m_far;
		consts.m_zSplitCountf = F32(getRenderer().getZSplitCount());
		consts.m_volumeSize = UVec3(m_volumeSize);
		consts.m_maxZSplitsToProcessf = F32(m_finalZSplit + 1);

		cmdb.setFastConstants(&consts, sizeof(consts));

		dispatchPPCompute(cmdb, 8, 8, m_volumeSize[0], m_volumeSize[1]);
	});
}

} // end namespace anki
