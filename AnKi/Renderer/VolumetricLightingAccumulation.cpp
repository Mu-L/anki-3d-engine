// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/VolumetricLightingAccumulation.h>
#include <AnKi/Renderer/ShadowMapping.h>
#include <AnKi/Renderer/IndirectDiffuseProbes.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/ClusterBinning.h>
#include <AnKi/Resource/ImageResource.h>
#include <AnKi/Core/CVarSet.h>
#include <AnKi/Scene/Components/SkyboxComponent.h>
#include <AnKi/Util/Tracer.h>

namespace anki {

NumericCVar<F32> g_volumetricLightingAccumulationQualityXYCVar(CVarSubsystem::kRenderer, "VolumetricLightingAccumulationQualityXY", 4.0f, 1.0f, 16.0f,
															   "Quality of XY dimensions of volumetric lights");
NumericCVar<F32> g_volumetricLightingAccumulationQualityZCVar(CVarSubsystem::kRenderer, "VolumetricLightingAccumulationQualityZ", 4.0f, 1.0f, 16.0f,
															  "Quality of Z dimension of volumetric lights");
NumericCVar<U32> g_volumetricLightingAccumulationFinalZSplitCVar(CVarSubsystem::kRenderer, "VolumetricLightingAccumulationFinalZSplit", 26, 1, 256,
																 "Final cluster split that will recieve volumetric lights");

Error VolumetricLightingAccumulation::init()
{
	// Misc
	const F32 qualityXY = g_volumetricLightingAccumulationQualityXYCVar.get();
	const F32 qualityZ = g_volumetricLightingAccumulationQualityZCVar.get();
	const U32 finalZSplit = min(getRenderer().getZSplitCount() - 1, g_volumetricLightingAccumulationFinalZSplitCVar.get());

	m_volumeSize[0] = U32(F32(getRenderer().getTileCounts().x()) * qualityXY);
	m_volumeSize[1] = U32(F32(getRenderer().getTileCounts().y()) * qualityXY);
	m_volumeSize[2] = U32(F32(finalZSplit + 1) * qualityZ);
	ANKI_R_LOGV("Initializing volumetric lighting accumulation. Size %ux%ux%u", m_volumeSize[0], m_volumeSize[1], m_volumeSize[2]);

	if(!isAligned(getRenderer().getTileCounts().x(), m_volumeSize[0]) || !isAligned(getRenderer().getTileCounts().y(), m_volumeSize[1])
	   || m_volumeSize[0] == 0 || m_volumeSize[1] == 0 || m_volumeSize[2] == 0)
	{
		ANKI_R_LOGE("Wrong input");
		return Error::kUserData;
	}

	ANKI_CHECK(ResourceManager::getSingleton().loadResource("EngineAssets/BlueNoise_Rgba8_64x64.png", m_noiseImage));

	// Shaders
	ANKI_CHECK(loadShaderProgram("ShaderBinaries/VolumetricLightingAccumulation.ankiprogbin", {{"ENABLE_SHADOWS", 1}}, m_prog, m_grProg));

	// Create RTs
	TextureInitInfo texinit = getRenderer().create2DRenderTargetInitInfo(
		m_volumeSize[0], m_volumeSize[1], Format::kR16G16B16A16_Sfloat,
		TextureUsageBit::kUavCompute | TextureUsageBit::kSrvFragment | TextureUsageBit::kSrvCompute, "VolLight");
	texinit.m_depth = m_volumeSize[2];
	texinit.m_type = TextureType::k3D;
	m_rtTextures[0] = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvFragment);
	m_rtTextures[1] = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvFragment);

	return Error::kNone;
}

void VolumetricLightingAccumulation::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(VolumetricLightingAccumulation);
	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	const U readRtIdx = getRenderer().getFrameCount() & 1;

	m_runCtx.m_rts[0] = rgraph.importRenderTarget(m_rtTextures[readRtIdx].get(), TextureUsageBit::kSrvFragment);
	m_runCtx.m_rts[1] = rgraph.importRenderTarget(m_rtTextures[!readRtIdx].get(), TextureUsageBit::kNone);

	NonGraphicsRenderPass& pass = rgraph.newNonGraphicsRenderPass("Vol light");

	pass.newTextureDependency(m_runCtx.m_rts[0], TextureUsageBit::kSrvCompute);
	pass.newTextureDependency(m_runCtx.m_rts[1], TextureUsageBit::kUavCompute);
	pass.newTextureDependency(getRenderer().getShadowMapping().getShadowmapRt(), TextureUsageBit::kSrvCompute);

	pass.newBufferDependency(getRenderer().getClusterBinning().getClustersBufferHandle(), BufferUsageBit::kSrvCompute);
	pass.newBufferDependency(getRenderer().getClusterBinning().getPackedObjectsBufferHandle(GpuSceneNonRenderableObjectType::kLight),
							 BufferUsageBit::kSrvCompute);
	pass.newBufferDependency(
		getRenderer().getClusterBinning().getPackedObjectsBufferHandle(GpuSceneNonRenderableObjectType::kGlobalIlluminationProbe),
		BufferUsageBit::kSrvCompute);
	pass.newBufferDependency(getRenderer().getClusterBinning().getPackedObjectsBufferHandle(GpuSceneNonRenderableObjectType::kFogDensityVolume),
							 BufferUsageBit::kSrvCompute);

	if(getRenderer().getIndirectDiffuseProbes().hasCurrentlyRefreshedVolumeRt())
	{
		pass.newTextureDependency(getRenderer().getIndirectDiffuseProbes().getCurrentlyRefreshedVolumeRt(), TextureUsageBit::kSrvCompute);
	}

	pass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
		ANKI_TRACE_SCOPED_EVENT(VolumetricLightingAccumulation);
		CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

		cmdb.bindShaderProgram(m_grProg.get());

		// Bind all
		cmdb.bindSampler(ANKI_REG(s0), getRenderer().getSamplers().m_trilinearRepeat.get());
		cmdb.bindSampler(ANKI_REG(s1), getRenderer().getSamplers().m_trilinearClamp.get());
		cmdb.bindSampler(ANKI_REG(s2), getRenderer().getSamplers().m_trilinearClampShadow.get());

		rgraphCtx.bindTexture(ANKI_REG(u0), m_runCtx.m_rts[1]);

		cmdb.bindTexture(ANKI_REG(t0), TextureView(&m_noiseImage->getTexture(), TextureSubresourceDesc::all()));

		rgraphCtx.bindTexture(ANKI_REG(t1), m_runCtx.m_rts[0]);

		cmdb.bindUniformBuffer(ANKI_REG(b0), ctx.m_globalRenderingUniformsBuffer);
		cmdb.bindStorageBuffer(ANKI_REG(t2), getRenderer().getClusterBinning().getPackedObjectsBuffer(GpuSceneNonRenderableObjectType::kLight));
		cmdb.bindStorageBuffer(ANKI_REG(t3), getRenderer().getClusterBinning().getPackedObjectsBuffer(GpuSceneNonRenderableObjectType::kLight));
		rgraphCtx.bindTexture(ANKI_REG(t4), getRenderer().getShadowMapping().getShadowmapRt());
		cmdb.bindStorageBuffer(ANKI_REG(t5),
							   getRenderer().getClusterBinning().getPackedObjectsBuffer(GpuSceneNonRenderableObjectType::kGlobalIlluminationProbe));
		cmdb.bindStorageBuffer(ANKI_REG(t6),
							   getRenderer().getClusterBinning().getPackedObjectsBuffer(GpuSceneNonRenderableObjectType::kFogDensityVolume));
		cmdb.bindStorageBuffer(ANKI_REG(t7), getRenderer().getClusterBinning().getClustersBuffer());

		const SkyboxComponent* sky = SceneGraph::getSingleton().getSkybox();

		VolumetricLightingUniforms unis;
		if(!sky)
		{
			unis.m_minHeight = 0.0f;
			unis.m_oneOverMaxMinusMinHeight = 0.0f;
			unis.m_densityAtMinHeight = 0.0f;
			unis.m_densityAtMaxHeight = 0.0f;
		}
		else if(sky->getHeightOfMaxFogDensity() > sky->getHeightOfMaxFogDensity())
		{
			unis.m_minHeight = sky->getHeightOfMinFogDensity();
			unis.m_oneOverMaxMinusMinHeight = 1.0f / (sky->getHeightOfMaxFogDensity() - unis.m_minHeight + kEpsilonf);
			unis.m_densityAtMinHeight = sky->getMinFogDensity();
			unis.m_densityAtMaxHeight = sky->getMaxFogDensity();
		}
		else
		{
			unis.m_minHeight = sky->getHeightOfMaxFogDensity();
			unis.m_oneOverMaxMinusMinHeight = 1.0f / (sky->getHeightOfMinFogDensity() - unis.m_minHeight + kEpsilonf);
			unis.m_densityAtMinHeight = sky->getMaxFogDensity();
			unis.m_densityAtMaxHeight = sky->getMinFogDensity();
		}
		unis.m_volumeSize = UVec3(m_volumeSize);

		const U32 finalZSplit = min(getRenderer().getZSplitCount() - 1, g_volumetricLightingAccumulationFinalZSplitCVar.get());
		unis.m_maxZSplitsToProcessf = F32(finalZSplit + 1);

		cmdb.setPushConstants(&unis, sizeof(unis));

		dispatchPPCompute(cmdb, 8, 8, 8, m_volumeSize[0], m_volumeSize[1], m_volumeSize[2]);
	});
}

} // end namespace anki
