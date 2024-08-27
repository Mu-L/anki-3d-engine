// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/VrsSriGeneration.h>
#include <AnKi/Renderer/Scale.h>
#include <AnKi/Renderer/Dbg.h>
#include <AnKi/Renderer/Utils/Drawer.h>
#include <AnKi/Renderer/Utils/HzbGenerator.h>
#include <AnKi/Util/Logger.h>
#include <AnKi/Util/Tracer.h>
#include <AnKi/Core/CVarSet.h>
#include <AnKi/Core/App.h>
#include <AnKi/Scene/Components/GlobalIlluminationProbeComponent.h>
#include <AnKi/Scene/Components/ReflectionProbeComponent.h>

namespace anki {

static NumericCVar<U32> g_hzbWidthCVar(CVarSubsystem::kRenderer, "HzbWidth", 512, 16, 4 * 1024, "HZB map width");
static NumericCVar<U32> g_hzbHeightCVar(CVarSubsystem::kRenderer, "HzbHeight", 256, 16, 4 * 1024, "HZB map height");
static BoolCVar g_gbufferVrsCVar(CVarSubsystem::kRenderer, "GBufferVrs", false, "Enable VRS in GBuffer");
static BoolCVar g_visualizeGiProbes(CVarSubsystem::kRenderer, "VisualizeGiProbes", false, "Visualize GI probes");
static BoolCVar g_visualizeReflectionProbes(CVarSubsystem::kRenderer, "VisualizeReflProbes", false, "Visualize reflection probes");

GBuffer::~GBuffer()
{
}

Error GBuffer::init()
{
	Error err = initInternal();

	if(err)
	{
		ANKI_R_LOGE("Failed to initialize g-buffer pass");
	}

	return err;
}

Error GBuffer::initInternal()
{
	ANKI_R_LOGV("Initializing GBuffer. Resolution %ux%u", getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y());

	// RTs
	static constexpr Array<const char*, 2> depthRtNames = {{"GBuffer depth #0", "GBuffer depth #1"}};
	for(U32 i = 0; i < 2; ++i)
	{
		const TextureUsageBit usage = TextureUsageBit::kAllSrv | TextureUsageBit::kAllRtvDsv;
		TextureInitInfo texinit =
			getRenderer().create2DRenderTargetInitInfo(getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y(),
													   getRenderer().getDepthNoStencilFormat(), usage, depthRtNames[i]);

		m_depthRts[i] = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvPixel);
	}

	static constexpr Array<const char*, kGBufferColorRenderTargetCount> rtNames = {{"GBuffer rt0", "GBuffer rt1", "GBuffer rt2", "GBuffer rt3"}};
	for(U i = 0; i < kGBufferColorRenderTargetCount; ++i)
	{
		m_colorRtDescrs[i] = getRenderer().create2DRenderTargetDescription(
			getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y(), kGBufferColorRenderTargetFormats[i], rtNames[i]);
		m_colorRtDescrs[i].bake();
	}

	{
		const TextureUsageBit usage = TextureUsageBit::kSrvCompute | TextureUsageBit::kUavCompute | TextureUsageBit::kSrvGeometry;

		TextureInitInfo texinit =
			getRenderer().create2DRenderTargetInitInfo(g_hzbWidthCVar.get(), g_hzbHeightCVar.get(), Format::kR32_Sfloat, usage, "GBuffer HZB");
		texinit.m_mipmapCount = U8(computeMaxMipmapCount2d(texinit.m_width, texinit.m_height));
		ClearValue clear;
		clear.m_colorf = {1.0f, 1.0f, 1.0f, 1.0f};
		m_hzbRt = getRenderer().createAndClearRenderTarget(texinit, TextureUsageBit::kSrvCompute, clear);
	}

	ANKI_CHECK(loadShaderProgram("ShaderBinaries/VisualizeGBufferNormal.ankiprogbin", m_visNormalProg, m_visNormalGrProg));
	ANKI_CHECK(
		loadShaderProgram("ShaderBinaries/GBufferVisualizeProbe.ankiprogbin", {{"PROBE_TYPE", 0}}, m_visualizeProbeProg, m_visualizeGiProbeGrProg));
	ANKI_CHECK(
		loadShaderProgram("ShaderBinaries/GBufferVisualizeProbe.ankiprogbin", {{"PROBE_TYPE", 1}}, m_visualizeProbeProg, m_visualizeReflProbeGrProg));

	return Error::kNone;
}

void GBuffer::importRenderTargets(RenderingContext& ctx)
{
	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	if(m_runCtx.m_crntFrameDepthRt.isValid()) [[likely]]
	{
		// Already imported once
		m_runCtx.m_crntFrameDepthRt = rgraph.importRenderTarget(m_depthRts[getRenderer().getFrameCount() & 1].get(), TextureUsageBit::kNone);
		m_runCtx.m_prevFrameDepthRt = rgraph.importRenderTarget(m_depthRts[(getRenderer().getFrameCount() + 1) & 1].get());

		m_runCtx.m_hzbRt = rgraph.importRenderTarget(m_hzbRt.get());
	}
	else
	{
		m_runCtx.m_crntFrameDepthRt = rgraph.importRenderTarget(m_depthRts[getRenderer().getFrameCount() & 1].get(), TextureUsageBit::kNone);
		m_runCtx.m_prevFrameDepthRt =
			rgraph.importRenderTarget(m_depthRts[(getRenderer().getFrameCount() + 1) & 1].get(), TextureUsageBit::kSrvPixel);

		m_runCtx.m_hzbRt = rgraph.importRenderTarget(m_hzbRt.get(), TextureUsageBit::kSrvCompute);
	}
}

void GBuffer::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(GBuffer);

	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	// Visibility
	GpuVisibilityOutput visOut;
	FrustumGpuVisibilityInput visIn;
	{
		const CommonMatrices& matrices = ctx.m_matrices;
		const Array<F32, kMaxLodCount - 1> lodDistances = {g_lod0MaxDistanceCVar.get(), g_lod1MaxDistanceCVar.get()};

		visIn.m_passesName = "GBuffer";
		visIn.m_technique = RenderingTechnique::kGBuffer;
		visIn.m_viewProjectionMatrix = matrices.m_viewProjection;
		visIn.m_lodReferencePoint = matrices.m_cameraTransform.getTranslationPart().xyz();
		visIn.m_lodDistances = lodDistances;
		visIn.m_rgraph = &rgraph;
		visIn.m_hzbRt = &m_runCtx.m_hzbRt;
		visIn.m_gatherAabbIndices = g_dbgCVar.get();
		visIn.m_viewportSize = getRenderer().getInternalResolution();
		visIn.m_twoPhaseOcclusionCulling = getRenderer().getMeshletRenderingType() != MeshletRenderingType::kNone;

		getRenderer().getGpuVisibility().populateRenderGraph(visIn, visOut);

		m_runCtx.m_visibleAaabbIndicesBuffer = visOut.m_visibleAaabbIndicesBuffer;
		m_runCtx.m_visibleAaabbIndicesBufferDepedency = visOut.m_dependency;
	}

	// Create RTs
	Array<RenderTargetHandle, kMaxColorRenderTargets> rts;
	for(U i = 0; i < kGBufferColorRenderTargetCount; ++i)
	{
		m_runCtx.m_colorRts[i] = rgraph.newRenderTarget(m_colorRtDescrs[i]);
		rts[i] = m_runCtx.m_colorRts[i];
	}

	// Create the GBuffer pass
	auto genGBuffer = [&](Bool firstPass) {
		GraphicsRenderPass& pass = rgraph.newGraphicsRenderPass((firstPass) ? "GBuffer" : "GBuffer 2nd phase");

		const TextureUsageBit rtUsage = (firstPass) ? TextureUsageBit::kRtvDsvWrite : (TextureUsageBit::kRtvDsvRead | TextureUsageBit::kRtvDsvWrite);
		for(U i = 0; i < kGBufferColorRenderTargetCount; ++i)
		{
			pass.newTextureDependency(m_runCtx.m_colorRts[i], rtUsage);
		}

		pass.newTextureDependency(m_runCtx.m_crntFrameDepthRt, rtUsage);

		pass.newBufferDependency(getRenderer().getGpuSceneBufferHandle(), BufferUsageBit::kSrvGeometry | BufferUsageBit::kSrvPixel);

		// Only add one depedency to the GPU visibility. No need to track all buffers
		if(visOut.containsDrawcalls())
		{
			pass.newBufferDependency(visOut.m_dependency, BufferUsageBit::kIndirectDraw | BufferUsageBit::kSrvGeometry);
		}
		else
		{
			// Weird, make a check
			ANKI_ASSERT(GpuSceneArrays::RenderableBoundingVolumeGBuffer::getSingleton().getElementCount() == 0);
		}

		const RenderTargetLoadOperation loadOp = (firstPass) ? RenderTargetLoadOperation::kClear : RenderTargetLoadOperation::kLoad;
		Array<GraphicsRenderPassTargetDesc, kGBufferColorRenderTargetCount> colorRti;
		for(U32 i = 0; i < 4; ++i)
		{
			colorRti[i].m_handle = rts[i];
			colorRti[i].m_loadOperation = loadOp;
		}
		colorRti[3].m_clearValue.m_colorf = {1.0f, 1.0f, 1.0f, 1.0f};
		GraphicsRenderPassTargetDesc depthRti(m_runCtx.m_crntFrameDepthRt);
		depthRti.m_loadOperation = loadOp;
		depthRti.m_clearValue.m_depthStencil.m_depth = 1.0f;
		depthRti.m_subresource.m_depthStencilAspect = DepthStencilAspectBit::kDepth;
		pass.setRenderpassInfo(WeakArray{colorRti}, &depthRti, 0, 0, kMaxU32, kMaxU32);

		pass.setWork([&ctx, visOut, this](RenderPassWorkContext& rgraphCtx) {
			ANKI_TRACE_SCOPED_EVENT(GBuffer);

			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			// Set some state, leave the rest to default
			cmdb.setViewport(0, 0, getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y());

			RenderableDrawerArguments args;
			args.m_viewMatrix = ctx.m_matrices.m_view;
			args.m_cameraTransform = ctx.m_matrices.m_cameraTransform;
			args.m_viewProjectionMatrix = ctx.m_matrices.m_viewProjectionJitter;
			args.m_previousViewProjectionMatrix = ctx.m_matrices.m_jitter * ctx.m_prevMatrices.m_viewProjection;
			args.m_sampler = getRenderer().getSamplers().m_trilinearRepeatAnisoResolutionScalingBias.get();
			args.m_renderingTechinuqe = RenderingTechnique::kGBuffer;
			args.m_viewport = UVec4(0, 0, getRenderer().getInternalResolution());

			args.fill(visOut);

			cmdb.setDepthCompareOperation(CompareOperation::kLessEqual);
			getRenderer().getRenderableDrawer().drawMdi(args, cmdb);

			{
				struct Consts
				{
					Mat4 m_viewProjMat;
					Mat4 m_invViewProjMat;

					Vec2 m_viewportSize;
					U32 m_probeIdx;
					F32 m_sphereRadius;

					Vec3 m_cameraPos;
					F32 m_pixelShift;
				};

				// Visualize GI probes
				if(g_visualizeGiProbes.get())
				{
					cmdb.bindShaderProgram(m_visualizeGiProbeGrProg.get());
					cmdb.bindSrv(0, 0, GpuSceneArrays::GlobalIlluminationProbe::getSingleton().getBufferView());

					for(const auto& probe : SceneGraph::getSingleton().getComponentArrays().getGlobalIlluminationProbes())
					{
						Consts* consts = allocateAndBindConstants<Consts>(cmdb, 0, 0);

						consts->m_viewProjMat = ctx.m_matrices.m_viewProjectionJitter;
						consts->m_invViewProjMat = ctx.m_matrices.m_invertedViewProjectionJitter;
						consts->m_viewportSize = Vec2(getRenderer().getInternalResolution());
						consts->m_probeIdx = probe.getGpuSceneAllocation().getIndex();
						consts->m_sphereRadius = 0.5f;
						consts->m_cameraPos = ctx.m_matrices.m_cameraTransform.getTranslationPart().xyz();
						consts->m_pixelShift = (getRenderer().getFrameCount() & 1) ? 1.0f : 0.0f;

						cmdb.draw(PrimitiveTopology::kTriangles, 6, probe.getCellCount());
					}
				}

				// Visualize refl probes
				if(g_visualizeReflectionProbes.get())
				{
					cmdb.bindShaderProgram(m_visualizeReflProbeGrProg.get());
					cmdb.bindSrv(0, 0, GpuSceneArrays::ReflectionProbe::getSingleton().getBufferView());

					for(const auto& probe : SceneGraph::getSingleton().getComponentArrays().getReflectionProbes())
					{
						Consts* consts = allocateAndBindConstants<Consts>(cmdb, 0, 0);

						consts->m_viewProjMat = ctx.m_matrices.m_viewProjectionJitter;
						consts->m_invViewProjMat = ctx.m_matrices.m_invertedViewProjectionJitter;
						consts->m_viewportSize = Vec2(getRenderer().getInternalResolution());
						consts->m_probeIdx = probe.getGpuSceneAllocation().getIndex();
						consts->m_sphereRadius = 0.5f;
						consts->m_cameraPos = ctx.m_matrices.m_cameraTransform.getTranslationPart().xyz();
						consts->m_pixelShift = (getRenderer().getFrameCount() & 1) ? 1.0f : 0.0f;

						cmdb.draw(PrimitiveTopology::kTriangles, 6);
					}
				}
			}
		});
	};

	genGBuffer(true);

	// HZB generation for the 3rd stage or next frame
	getRenderer().getHzbGenerator().populateRenderGraph(m_runCtx.m_crntFrameDepthRt, getRenderer().getInternalResolution(), m_runCtx.m_hzbRt,
														UVec2(m_hzbRt->getWidth(), m_hzbRt->getHeight()), rgraph);

	// 2nd phase
	if(visIn.m_twoPhaseOcclusionCulling)
	{
		// Visibility (again)
		getRenderer().getGpuVisibility().populateRenderGraphStage3(visIn, visOut);

		// GBuffer again
		genGBuffer(false);

		// HZB generation for the next frame
		getRenderer().getHzbGenerator().populateRenderGraph(m_runCtx.m_crntFrameDepthRt, getRenderer().getInternalResolution(), m_runCtx.m_hzbRt,
															UVec2(m_hzbRt->getWidth(), m_hzbRt->getHeight()), rgraph);
	}
}

void GBuffer::getDebugRenderTarget(CString rtName, Array<RenderTargetHandle, kMaxDebugRenderTargets>& handles,
								   ShaderProgramPtr& optionalShaderProgram) const
{
	if(rtName == "GBufferAlbedo")
	{
		handles[0] = m_runCtx.m_colorRts[0];
	}
	else if(rtName == "GBufferNormals")
	{
		handles[0] = m_runCtx.m_colorRts[2];
		optionalShaderProgram = m_visNormalGrProg;
	}
	else if(rtName == "GBufferVelocity")
	{
		handles[0] = m_runCtx.m_colorRts[3];
	}
	else
	{
		ANKI_ASSERT(!"See file");
	}
}

} // end namespace anki
