// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/RtShadows.h>
#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/ShadowMapping.h>
#include <AnKi/Renderer/AccelerationStructureBuilder.h>
#include <AnKi/Renderer/MotionVectors.h>
#include <AnKi/Renderer/DepthDownscale.h>
#include <AnKi/Renderer/ClusterBinning.h>
#include <AnKi/Util/Tracer.h>
#include <AnKi/Core/CVarSet.h>
#include <AnKi/Shaders/Include/MaterialTypes.h>
#include <AnKi/Shaders/Include/GpuSceneTypes.h>
#include <AnKi/Core/GpuMemory/UnifiedGeometryBuffer.h>
#include <AnKi/Core/GpuMemory/GpuSceneBuffer.h>
#include <AnKi/Core/GpuMemory/GpuVisibleTransientMemoryPool.h>

namespace anki {

static BoolCVar g_rtShadowsSvgfCVar(CVarSubsystem::kRenderer, "RtShadowsSvgf", false, "Enable or not RT shadows SVGF");
static NumericCVar<U8> g_rtShadowsSvgfAtrousPassCountCVar(CVarSubsystem::kRenderer, "RtShadowsSvgfAtrousPassCount", 3, 1, 20,
														  "Number of atrous passes of SVGF");
static NumericCVar<U32> g_rtShadowsRaysPerPixelCVar(CVarSubsystem::kRenderer, "RtShadowsRaysPerPixel", 1, 1, 8, "Number of shadow rays per pixel");

Error RtShadows::init()
{
	const Error err = initInternal();
	if(err)
	{
		ANKI_R_LOGE("Failed to initialize ray traced shadows");
	}

	return err;
}

Error RtShadows::initInternal()
{
	ANKI_R_LOGV("Initializing RT shadows");

	m_useSvgf = g_rtShadowsSvgfCVar.get();
	m_atrousPassCount = g_rtShadowsSvgfAtrousPassCountCVar.get();

	ANKI_CHECK(ResourceManager::getSingleton().loadResource("EngineAssets/BlueNoise_Rgba8_64x64.png", m_blueNoiseImage));

	ANKI_CHECK(loadShaderProgram("ShaderBinaries/RtShadowsSetupSbtBuild.ankiprogbin", m_setupBuildSbtProg, m_setupBuildSbtGrProg));
	ANKI_CHECK(loadShaderProgram("ShaderBinaries/RtShadowsSbtBuild.ankiprogbin", m_buildSbtProg, m_buildSbtGrProg));
	ANKI_CHECK(ResourceManager::getSingleton().loadResource("ShaderBinaries/RtShadows.ankiprogbin", m_rayGenAndMissProg));

	// Ray gen and miss
	{
		ShaderProgramResourceVariantInitInfo variantInitInfo(m_rayGenAndMissProg);
		variantInitInfo.addMutation("RAYS_PER_PIXEL", g_rtShadowsRaysPerPixelCVar.get());
		variantInitInfo.requestTechniqueAndTypes(ShaderTypeBit::kRayGen, "RtShadows");
		const ShaderProgramResourceVariant* variant;
		m_rayGenAndMissProg->getOrCreateVariant(variantInitInfo, variant);
		m_rtLibraryGrProg.reset(&variant->getProgram());
		m_rayGenShaderGroupIdx = variant->getShaderGroupHandleIndex();

		ShaderProgramResourceVariantInitInfo variantInitInfo2(m_rayGenAndMissProg);
		variantInitInfo2.addMutation("RAYS_PER_PIXEL", g_rtShadowsRaysPerPixelCVar.get());
		variantInitInfo2.requestTechniqueAndTypes(ShaderTypeBit::kMiss, "RtShadows");
		m_rayGenAndMissProg->getOrCreateVariant(variantInitInfo2, variant);
		m_missShaderGroupIdx = variant->getShaderGroupHandleIndex();
	}

	// Denoise program
	if(!m_useSvgf)
	{
		ANKI_CHECK(
			loadShaderProgram("ShaderBinaries/RtShadowsDenoise.ankiprogbin", {{"BLUR_ORIENTATION", 0}}, m_denoiseProg, m_grDenoiseHorizontalProg));

		ANKI_CHECK(
			loadShaderProgram("ShaderBinaries/RtShadowsDenoise.ankiprogbin", {{"BLUR_ORIENTATION", 1}}, m_denoiseProg, m_grDenoiseVerticalProg));
	}

	// SVGF variance program
	if(m_useSvgf)
	{
		ANKI_CHECK(loadShaderProgram("ShaderBinaries/RtShadowsSvgfVariance.ankiprogbin", m_svgfVarianceProg, m_svgfVarianceGrProg));
	}

	// SVGF atrous program
	if(m_useSvgf)
	{
		ANKI_CHECK(loadShaderProgram("ShaderBinaries/RtShadowsSvgfAtrous.ankiprogbin", {{"LAST_PASS", 0}}, m_svgfAtrousProg, m_svgfAtrousGrProg));
		ANKI_CHECK(
			loadShaderProgram("ShaderBinaries/RtShadowsSvgfAtrous.ankiprogbin", {{"LAST_PASS", 1}}, m_svgfAtrousProg, m_svgfAtrousLastPassGrProg));
	}

	// Upscale program
	ANKI_CHECK(loadShaderProgram("ShaderBinaries/RtShadowsUpscale.ankiprogbin", m_upscaleProg, m_upscaleGrProg));

	// Quarter rez shadow RT
	{
		TextureInitInfo texinit = getRenderer().create2DRenderTargetInitInfo(
			getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2, Format::kR8_Unorm,
			TextureUsageBit::kAllSrv | TextureUsageBit::kUavTraceRays | TextureUsageBit::kUavCompute, "RtShadows History");
		m_historyRt = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvFragment);
	}

	// Temp shadow RT
	{
		m_intermediateShadowsRtDescr = getRenderer().create2DRenderTargetDescription(
			getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2, Format::kR8_Unorm, "RtShadows Tmp");
		m_intermediateShadowsRtDescr.bake();
	}

	// Moments RT
	{
		TextureInitInfo texinit = getRenderer().create2DRenderTargetInitInfo(
			getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2, Format::kR32G32_Sfloat,
			TextureUsageBit::kAllSrv | TextureUsageBit::kUavTraceRays | TextureUsageBit::kUavCompute, "RtShadows Moments #1");
		m_momentsRts[0] = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvFragment);

		texinit.setName("RtShadows Moments #2");
		m_momentsRts[1] = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvFragment);
	}

	// Variance RT
	if(m_useSvgf)
	{
		m_varianceRtDescr = getRenderer().create2DRenderTargetDescription(
			getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2, Format::kR32_Sfloat, "RtShadows Variance");
		m_varianceRtDescr.bake();
	}

	// Final RT
	{
		m_upscaledRtDescr = getRenderer().create2DRenderTargetDescription(
			getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y(), Format::kR8_Unorm, "RtShadows Upscaled");
		m_upscaledRtDescr.bake();
	}

	{
		TextureInitInfo texinit = getRenderer().create2DRenderTargetInitInfo(
			getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2, Format::kR32_Sfloat,
			TextureUsageBit::kAllSrv | TextureUsageBit::kUavTraceRays | TextureUsageBit::kUavCompute, "RtShadows history len");
		ClearValue clear;
		clear.m_colorf[0] = 1.0f;
		m_dummyHistoryLenTex = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvFragment, clear);
	}

	// Misc
	m_sbtRecordSize = getAlignedRoundUp(GrManager::getSingleton().getDeviceCapabilities().m_sbtRecordAlignment, m_sbtRecordSize);

	return Error::kNone;
}

void RtShadows::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(RtShadows);

#define ANKI_DEPTH_DEP \
	getRenderer().getDepthDownscale().getRt(), TextureUsageBit::kSrvTraceRays | TextureUsageBit::kSrvCompute, \
		DepthDownscale::kQuarterInternalResolution

	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	// Import RTs
	{
		const U32 prevRtIdx = getRenderer().getFrameCount() & 1;

		if(!m_rtsImportedOnce) [[unlikely]]
		{
			m_runCtx.m_historyRt = rgraph.importRenderTarget(m_historyRt.get(), TextureUsageBit::kSrvFragment);

			m_runCtx.m_prevMomentsRt = rgraph.importRenderTarget(m_momentsRts[prevRtIdx].get(), TextureUsageBit::kSrvFragment);

			m_rtsImportedOnce = true;
		}
		else
		{
			m_runCtx.m_historyRt = rgraph.importRenderTarget(m_historyRt.get());
			m_runCtx.m_prevMomentsRt = rgraph.importRenderTarget(m_momentsRts[prevRtIdx].get());
		}

		if((getPassCountWithoutUpscaling() % 2) == 1)
		{
			m_runCtx.m_intermediateShadowsRts[0] = rgraph.newRenderTarget(m_intermediateShadowsRtDescr);
			m_runCtx.m_intermediateShadowsRts[1] = rgraph.newRenderTarget(m_intermediateShadowsRtDescr);
		}
		else
		{
			// We can save a render target if we have even number of renderpasses
			m_runCtx.m_intermediateShadowsRts[0] = rgraph.newRenderTarget(m_intermediateShadowsRtDescr);
			m_runCtx.m_intermediateShadowsRts[1] = m_runCtx.m_historyRt;
		}

		m_runCtx.m_currentMomentsRt = rgraph.importRenderTarget(m_momentsRts[!prevRtIdx].get(), TextureUsageBit::kNone);

		if(m_useSvgf)
		{
			if(m_atrousPassCount > 1)
			{
				m_runCtx.m_varianceRts[0] = rgraph.newRenderTarget(m_varianceRtDescr);
			}
			m_runCtx.m_varianceRts[1] = rgraph.newRenderTarget(m_varianceRtDescr);
		}

		m_runCtx.m_upscaledRt = rgraph.newRenderTarget(m_upscaledRtDescr);
	}

	// Setup build SBT dispatch
	BufferHandle sbtBuildIndirectArgsHandle;
	BufferView sbtBuildIndirectArgsBuffer;
	{
		sbtBuildIndirectArgsBuffer = GpuVisibleTransientMemoryPool::getSingleton().allocateStructuredBuffer<DispatchIndirectArgs>(1);
		sbtBuildIndirectArgsHandle = rgraph.importBuffer(sbtBuildIndirectArgsBuffer, BufferUsageBit::kUavCompute);

		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows setup build SBT");

		rpass.newBufferDependency(sbtBuildIndirectArgsHandle, BufferUsageBit::kAccelerationStructureBuild);

		rpass.setWork([this, sbtBuildIndirectArgsBuffer](RenderPassWorkContext& rgraphCtx) {
			ANKI_TRACE_SCOPED_EVENT(RtShadows);
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			cmdb.bindShaderProgram(m_setupBuildSbtGrProg.get());

			cmdb.bindStorageBuffer(ANKI_REG(t0), GpuSceneArrays::RenderableBoundingVolumeRt::getSingleton().getBufferView());
			cmdb.bindStorageBuffer(ANKI_REG(u0), sbtBuildIndirectArgsBuffer);

			cmdb.dispatchCompute(1, 1, 1);
		});
	}

	// Build the SBT
	BufferHandle sbtHandle;
	BufferView sbtBuffer;
	{
		// Allocate SBT
		U8* sbtMem;
		sbtBuffer = RebarTransientMemoryPool::getSingleton().allocateFrame(
			(GpuSceneArrays::RenderableBoundingVolumeRt::getSingleton().getElementCount() + 2) * m_sbtRecordSize, sbtMem);
		sbtHandle = rgraph.importBuffer(sbtBuffer, BufferUsageBit::kUavCompute);

		// Write the first 2 entries of the SBT
		ConstWeakArray<U8> shaderGroupHandles = m_rtLibraryGrProg->getShaderGroupHandles();
		const U32 shaderHandleSize = GrManager::getSingleton().getDeviceCapabilities().m_shaderGroupHandleSize;
		memcpy(sbtMem, &shaderGroupHandles[m_rayGenShaderGroupIdx * shaderHandleSize], shaderHandleSize);
		memcpy(sbtMem + m_sbtRecordSize, &shaderGroupHandles[m_missShaderGroupIdx * shaderHandleSize], shaderHandleSize);

		// Create the pass
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows build SBT");

		BufferHandle visibilityHandle;
		BufferView visibleRenderableIndicesBuff;
		getRenderer().getAccelerationStructureBuilder().getVisibilityInfo(visibilityHandle, visibleRenderableIndicesBuff);

		rpass.newBufferDependency(visibilityHandle, BufferUsageBit::kSrvCompute);
		rpass.newBufferDependency(sbtBuildIndirectArgsHandle, BufferUsageBit::kIndirectCompute);

		rpass.setWork([this, sbtBuildIndirectArgsBuffer, sbtBuffer, visibleRenderableIndicesBuff](RenderPassWorkContext& rgraphCtx) {
			ANKI_TRACE_SCOPED_EVENT(RtShadows);
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			cmdb.bindShaderProgram(m_buildSbtGrProg.get());

			cmdb.bindStorageBuffer(ANKI_REG(t0), GpuSceneArrays::Renderable::getSingleton().getBufferView());
			cmdb.bindStorageBuffer(ANKI_REG(t1), BufferView(&GpuSceneBuffer::getSingleton().getBuffer()));
			cmdb.bindStorageBuffer(ANKI_REG(t2), visibleRenderableIndicesBuff);
			cmdb.bindStorageBuffer(ANKI_REG(t3), BufferView(&m_rtLibraryGrProg->getShaderGroupHandlesGpuBuffer()));
			cmdb.bindStorageBuffer(ANKI_REG(u0), sbtBuffer);

			RtShadowsSbtBuildUniforms unis = {};
			ANKI_ASSERT(m_sbtRecordSize % 4 == 0);
			unis.m_sbtRecordDwordSize = m_sbtRecordSize / 4;
			const U32 shaderHandleSize = GrManager::getSingleton().getDeviceCapabilities().m_shaderGroupHandleSize;
			ANKI_ASSERT(shaderHandleSize % 4 == 0);
			unis.m_shaderHandleDwordSize = shaderHandleSize / 4;
			cmdb.setPushConstants(&unis, sizeof(unis));

			cmdb.dispatchComputeIndirect(sbtBuildIndirectArgsBuffer);
		});
	}

	// Ray gen
	{
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows");

		rpass.newTextureDependency(m_runCtx.m_historyRt, TextureUsageBit::kSrvTraceRays);
		rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[0], TextureUsageBit::kUavTraceRays);
		rpass.newAccelerationStructureDependency(getRenderer().getAccelerationStructureBuilder().getAccelerationStructureHandle(),
												 AccelerationStructureUsageBit::kTraceRaysRead);
		rpass.newTextureDependency(ANKI_DEPTH_DEP);
		rpass.newTextureDependency(getRenderer().getMotionVectors().getMotionVectorsRt(), TextureUsageBit::kSrvTraceRays);
		rpass.newTextureDependency(getRenderer().getGBuffer().getColorRt(2), TextureUsageBit::kSrvTraceRays);

		rpass.newTextureDependency(m_runCtx.m_prevMomentsRt, TextureUsageBit::kSrvTraceRays);
		rpass.newTextureDependency(m_runCtx.m_currentMomentsRt, TextureUsageBit::kUavTraceRays);

		rpass.newBufferDependency(getRenderer().getClusterBinning().getClustersBufferHandle(), BufferUsageBit::kSrvTraceRays);

		rpass.setWork([this, sbtBuffer, &ctx](RenderPassWorkContext& rgraphCtx) {
			ANKI_TRACE_SCOPED_EVENT(RtShadows);
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			cmdb.bindShaderProgram(m_rtLibraryGrProg.get());

			// Allocate, set and bind global uniforms
			{
				MaterialGlobalUniforms* globalUniforms;
				const RebarAllocation globalUniformsToken = RebarTransientMemoryPool::getSingleton().allocateFrame(1, globalUniforms);

				memset(globalUniforms, 0, sizeof(*globalUniforms)); // Don't care for now

				cmdb.bindUniformBuffer(ANKI_REG(ANKI_MATERIAL_REGISTER_GLOBAL_UNIFORMS), globalUniformsToken);
			}

			// More globals
			cmdb.bindSampler(ANKI_REG(ANKI_MATERIAL_REGISTER_TILINEAR_REPEAT_SAMPLER), getRenderer().getSamplers().m_trilinearRepeat.get());
			cmdb.bindStorageBuffer(ANKI_REG(ANKI_MATERIAL_REGISTER_GPU_SCENE), GpuSceneBuffer::getSingleton().getBufferView());

#define ANKI_UNIFIED_GEOM_FORMAT(fmt, shaderType, reg) \
	cmdb.bindTexelBuffer( \
		ANKI_REG(reg), \
		BufferView(&UnifiedGeometryBuffer::getSingleton().getBuffer(), 0, \
				   getAlignedRoundDown(getFormatInfo(Format::k##fmt).m_texelSize, UnifiedGeometryBuffer::getSingleton().getBuffer().getSize())), \
		Format::k##fmt);
#include <AnKi/Shaders/Include/UnifiedGeometryTypes.def.h>

			cmdb.bindUniformBuffer(ANKI_REG2(b0, space2), ctx.m_globalRenderingUniformsBuffer);

			cmdb.bindSampler(ANKI_REG2(s0, space2), getRenderer().getSamplers().m_trilinearRepeat.get());

			rgraphCtx.bindTexture(ANKI_REG2(u0, space2), m_runCtx.m_intermediateShadowsRts[0]);
			rgraphCtx.bindTexture(ANKI_REG2(t0, space2), m_runCtx.m_historyRt);
			cmdb.bindSampler(ANKI_REG2(s1, space2), getRenderer().getSamplers().m_trilinearClamp.get());
			rgraphCtx.bindTexture(ANKI_REG2(t1, space2), getRenderer().getDepthDownscale().getRt(), DepthDownscale::kQuarterInternalResolution);
			rgraphCtx.bindTexture(ANKI_REG2(t2, space2), getRenderer().getMotionVectors().getMotionVectorsRt());
			cmdb.bindTexture(ANKI_REG2(t3, space2), TextureView(m_dummyHistoryLenTex.get(), TextureSubresourceDesc::all()));
			rgraphCtx.bindTexture(ANKI_REG2(t4, space2), getRenderer().getGBuffer().getColorRt(2));
			rgraphCtx.bindAccelerationStructure(ANKI_REG2(t5, space2),
												getRenderer().getAccelerationStructureBuilder().getAccelerationStructureHandle());
			rgraphCtx.bindTexture(ANKI_REG2(t6, space2), m_runCtx.m_prevMomentsRt);
			rgraphCtx.bindTexture(ANKI_REG2(u1, space2), m_runCtx.m_currentMomentsRt);
			cmdb.bindTexture(ANKI_REG2(t7, space2), TextureView(&m_blueNoiseImage->getTexture(), TextureSubresourceDesc::all()));

			cmdb.traceRays(sbtBuffer, m_sbtRecordSize, GpuSceneArrays::RenderableBoundingVolumeRt::getSingleton().getElementCount(), 1,
						   getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2, 1);
		});
	}

	// Denoise pass horizontal
	if(!m_useSvgf)
	{
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows Denoise Horizontal");

		rpass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
			runDenoise(ctx, rgraphCtx, true);
		});

		rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[0], TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(ANKI_DEPTH_DEP);
		rpass.newTextureDependency(getRenderer().getGBuffer().getColorRt(2), TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(m_runCtx.m_currentMomentsRt, TextureUsageBit::kSrvCompute);

		rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[1], TextureUsageBit::kUavCompute);
	}

	// Denoise pass vertical
	if(!m_useSvgf)
	{
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows Denoise Vertical");
		rpass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
			runDenoise(ctx, rgraphCtx, false);
		});

		rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[1], TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(ANKI_DEPTH_DEP);
		rpass.newTextureDependency(getRenderer().getGBuffer().getColorRt(2), TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(m_runCtx.m_currentMomentsRt, TextureUsageBit::kSrvCompute);

		rpass.newTextureDependency(m_runCtx.m_historyRt, TextureUsageBit::kUavCompute);
	}

	// Variance calculation pass
	if(m_useSvgf)
	{
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows SVGF Variance");

		rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[0], TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(m_runCtx.m_currentMomentsRt, TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(ANKI_DEPTH_DEP);
		rpass.newTextureDependency(getRenderer().getGBuffer().getColorRt(2), TextureUsageBit::kSrvCompute);

		rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[1], TextureUsageBit::kUavCompute);
		rpass.newTextureDependency(m_runCtx.m_varianceRts[1], TextureUsageBit::kUavCompute);

		rpass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
			ANKI_TRACE_SCOPED_EVENT(RtShadows);
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			cmdb.bindShaderProgram(m_svgfVarianceGrProg.get());

			cmdb.bindSampler(ANKI_REG(s0), getRenderer().getSamplers().m_trilinearClamp.get());

			rgraphCtx.bindTexture(ANKI_REG(t0), m_runCtx.m_intermediateShadowsRts[0]);
			rgraphCtx.bindTexture(ANKI_REG(t1), m_runCtx.m_currentMomentsRt);
			cmdb.bindTexture(ANKI_REG(t2), TextureView(m_dummyHistoryLenTex.get(), TextureSubresourceDesc::all()));
			rgraphCtx.bindTexture(ANKI_REG(t3), getRenderer().getDepthDownscale().getRt(), DepthDownscale::kQuarterInternalResolution);

			rgraphCtx.bindTexture(ANKI_REG(u0), m_runCtx.m_intermediateShadowsRts[1]);
			rgraphCtx.bindTexture(ANKI_REG(u1), m_runCtx.m_varianceRts[1]);

			const Mat4& invProjMat = ctx.m_matrices.m_projectionJitter.getInverse();
			cmdb.setPushConstants(&invProjMat, sizeof(invProjMat));

			dispatchPPCompute(cmdb, 8, 8, getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2);
		});
	}

	// SVGF Atrous
	if(m_useSvgf)
	{
		for(U32 i = 0; i < m_atrousPassCount; ++i)
		{
			const Bool lastPass = i == U32(m_atrousPassCount - 1);
			const U32 readRtIdx = (i + 1) & 1;

			NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows SVGF Atrous");

			rpass.newTextureDependency(ANKI_DEPTH_DEP);
			rpass.newTextureDependency(getRenderer().getGBuffer().getColorRt(2), TextureUsageBit::kSrvCompute);
			rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[readRtIdx], TextureUsageBit::kSrvCompute);
			rpass.newTextureDependency(m_runCtx.m_varianceRts[readRtIdx], TextureUsageBit::kSrvCompute);

			if(!lastPass)
			{
				rpass.newTextureDependency(m_runCtx.m_intermediateShadowsRts[!readRtIdx], TextureUsageBit::kUavCompute);

				rpass.newTextureDependency(m_runCtx.m_varianceRts[!readRtIdx], TextureUsageBit::kUavCompute);
			}
			else
			{
				rpass.newTextureDependency(m_runCtx.m_historyRt, TextureUsageBit::kUavCompute);
			}

			rpass.setWork([this, &ctx, passIdx = i](RenderPassWorkContext& rgraphCtx) {
				ANKI_TRACE_SCOPED_EVENT(RtShadows);
				CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

				const Bool lastPass = passIdx == U32(m_atrousPassCount - 1);
				const U32 readRtIdx = (passIdx + 1) & 1;

				if(lastPass)
				{
					cmdb.bindShaderProgram(m_svgfAtrousLastPassGrProg.get());
				}
				else
				{
					cmdb.bindShaderProgram(m_svgfAtrousGrProg.get());
				}

				cmdb.bindSampler(ANKI_REG(s0), getRenderer().getSamplers().m_nearestNearestClamp.get());

				rgraphCtx.bindTexture(ANKI_REG(t0), getRenderer().getDepthDownscale().getRt(), DepthDownscale::kQuarterInternalResolution);
				rgraphCtx.bindTexture(ANKI_REG(t1), m_runCtx.m_intermediateShadowsRts[readRtIdx]);
				rgraphCtx.bindTexture(ANKI_REG(t2), m_runCtx.m_varianceRts[readRtIdx]);

				if(!lastPass)
				{
					rgraphCtx.bindTexture(ANKI_REG(u0), m_runCtx.m_intermediateShadowsRts[!readRtIdx]);
					rgraphCtx.bindTexture(ANKI_REG(u1), m_runCtx.m_varianceRts[!readRtIdx]);
				}
				else
				{
					rgraphCtx.bindTexture(ANKI_REG(u0), m_runCtx.m_historyRt);
				}

				const Mat4& invProjMat = ctx.m_matrices.m_projectionJitter.getInverse();
				cmdb.setPushConstants(&invProjMat, sizeof(invProjMat));

				dispatchPPCompute(cmdb, 8, 8, getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2);
			});
		}
	}

	// Upscale
	{
		NonGraphicsRenderPass& rpass = rgraph.newNonGraphicsRenderPass("RtShadows Upscale");

		rpass.newTextureDependency(m_runCtx.m_historyRt, TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(getRenderer().getGBuffer().getDepthRt(), TextureUsageBit::kSrvCompute);
		rpass.newTextureDependency(ANKI_DEPTH_DEP);

		rpass.newTextureDependency(m_runCtx.m_upscaledRt, TextureUsageBit::kUavCompute);

		rpass.setWork([this](RenderPassWorkContext& rgraphCtx) {
			ANKI_TRACE_SCOPED_EVENT(RtShadows);
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			cmdb.bindShaderProgram(m_upscaleGrProg.get());

			cmdb.bindSampler(ANKI_REG(s0), getRenderer().getSamplers().m_trilinearClamp.get());

			rgraphCtx.bindTexture(ANKI_REG(t0), m_runCtx.m_historyRt);
			rgraphCtx.bindTexture(ANKI_REG(u0), m_runCtx.m_upscaledRt);
			rgraphCtx.bindTexture(ANKI_REG(t1), getRenderer().getDepthDownscale().getRt(), DepthDownscale::kQuarterInternalResolution);
			rgraphCtx.bindTexture(ANKI_REG(t2), getRenderer().getGBuffer().getDepthRt());

			dispatchPPCompute(cmdb, 8, 8, getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y());
		});
	}
}

void RtShadows::runDenoise(const RenderingContext& ctx, RenderPassWorkContext& rgraphCtx, Bool horizontal)
{
	ANKI_TRACE_SCOPED_EVENT(RtShadows);
	CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

	cmdb.bindShaderProgram((horizontal) ? m_grDenoiseHorizontalProg.get() : m_grDenoiseVerticalProg.get());

	cmdb.bindSampler(ANKI_REG(s0), getRenderer().getSamplers().m_nearestNearestClamp.get());
	rgraphCtx.bindTexture(ANKI_REG(t0), m_runCtx.m_intermediateShadowsRts[(horizontal) ? 0 : 1]);
	rgraphCtx.bindTexture(ANKI_REG(t1), getRenderer().getDepthDownscale().getRt(), DepthDownscale::kQuarterInternalResolution);
	rgraphCtx.bindTexture(ANKI_REG(t2), getRenderer().getGBuffer().getColorRt(2));
	rgraphCtx.bindTexture(ANKI_REG(t3), m_runCtx.m_currentMomentsRt);
	cmdb.bindTexture(ANKI_REG(t4), TextureView(m_dummyHistoryLenTex.get(), TextureSubresourceDesc::all()));

	rgraphCtx.bindTexture(ANKI_REG(u0), (horizontal) ? m_runCtx.m_intermediateShadowsRts[1] : m_runCtx.m_historyRt);

	RtShadowsDenoiseUniforms consts;
	consts.m_invViewProjMat = ctx.m_matrices.m_invertedViewProjectionJitter;
	consts.m_time = F32(GlobalFrameIndex::getSingleton().m_value % 0xFFFFu);
	consts.m_minSampleCount = 8;
	consts.m_maxSampleCount = 32;
	cmdb.setPushConstants(&consts, sizeof(consts));

	dispatchPPCompute(cmdb, 8, 8, getRenderer().getInternalResolution().x() / 2, getRenderer().getInternalResolution().y() / 2);
}

void RtShadows::getDebugRenderTarget([[maybe_unused]] CString rtName, Array<RenderTargetHandle, kMaxDebugRenderTargets>& handles,
									 [[maybe_unused]] ShaderProgramPtr& optionalShaderProgram) const
{
	handles[0] = m_runCtx.m_upscaledRt;
}

} // end namespace anki
