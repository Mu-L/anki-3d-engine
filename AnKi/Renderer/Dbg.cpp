// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/Dbg.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/LightShading.h>
#include <AnKi/Renderer/FinalComposite.h>
#include <AnKi/Renderer/ForwardShading.h>
#include <AnKi/Renderer/ClusterBinning.h>
#include <AnKi/Renderer/PrimaryNonRenderableVisibility.h>
#include <AnKi/Scene.h>
#include <AnKi/Util/Logger.h>
#include <AnKi/Util/Enum.h>
#include <AnKi/Util/Tracer.h>
#include <AnKi/Core/CVarSet.h>
#include <AnKi/Collision/ConvexHullShape.h>
#include <AnKi/Physics/PhysicsWorld.h>

namespace anki {

BoolCVar g_dbgCVar(CVarSubsystem::kRenderer, "Dbg", false, "Enable or not debug visualization");
static BoolCVar g_dbgPhysicsCVar(CVarSubsystem::kRenderer, "DbgPhysics", false, "Enable or not physics debug visualization");

Dbg::Dbg()
{
}

Dbg::~Dbg()
{
}

Error Dbg::init()
{
	ANKI_R_LOGV("Initializing DBG");

	// RT descr
	m_rtDescr = getRenderer().create2DRenderTargetDescription(getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y(),
															  Format::kR8G8B8A8_Unorm, "Dbg");
	m_rtDescr.bake();

	ResourceManager& rsrcManager = ResourceManager::getSingleton();
	ANKI_CHECK(rsrcManager.loadResource("EngineAssets/GiProbe.ankitex", m_giProbeImage));
	ANKI_CHECK(rsrcManager.loadResource("EngineAssets/LightBulb.ankitex", m_pointLightImage));
	ANKI_CHECK(rsrcManager.loadResource("EngineAssets/SpotLight.ankitex", m_spotLightImage));
	ANKI_CHECK(rsrcManager.loadResource("EngineAssets/GreenDecal.ankitex", m_decalImage));
	ANKI_CHECK(rsrcManager.loadResource("EngineAssets/Mirror.ankitex", m_reflectionImage));

	ANKI_CHECK(rsrcManager.loadResource("ShaderBinaries/DbgRenderables.ankiprogbin", m_renderablesProg));
	ANKI_CHECK(rsrcManager.loadResource("ShaderBinaries/DbgBillboard.ankiprogbin", m_nonRenderablesProg));

	{
		BufferInitInfo buffInit("Dbg cube verts");
		buffInit.m_size = sizeof(Vec3) * 8;
		buffInit.m_usage = BufferUsageBit::kVertex;
		buffInit.m_mapAccess = BufferMapAccessBit::kWrite;
		m_cubeVertsBuffer = GrManager::getSingleton().newBuffer(buffInit);

		Vec3* verts = static_cast<Vec3*>(m_cubeVertsBuffer->map(0, kMaxPtrSize, BufferMapAccessBit::kWrite));

		constexpr F32 kSize = 1.0f;
		verts[0] = Vec3(kSize, kSize, kSize); // front top right
		verts[1] = Vec3(-kSize, kSize, kSize); // front top left
		verts[2] = Vec3(-kSize, -kSize, kSize); // front bottom left
		verts[3] = Vec3(kSize, -kSize, kSize); // front bottom right
		verts[4] = Vec3(kSize, kSize, -kSize); // back top right
		verts[5] = Vec3(-kSize, kSize, -kSize); // back top left
		verts[6] = Vec3(-kSize, -kSize, -kSize); // back bottom left
		verts[7] = Vec3(kSize, -kSize, -kSize); // back bottom right

		m_cubeVertsBuffer->unmap();

		constexpr U kIndexCount = 12 * 2;
		buffInit.setName("Dbg cube indices");
		buffInit.m_usage = BufferUsageBit::kIndex;
		buffInit.m_size = kIndexCount * sizeof(U16);
		m_cubeIndicesBuffer = GrManager::getSingleton().newBuffer(buffInit);
		U16* indices = static_cast<U16*>(m_cubeIndicesBuffer->map(0, kMaxPtrSize, BufferMapAccessBit::kWrite));

		U c = 0;
		indices[c++] = 0;
		indices[c++] = 1;
		indices[c++] = 1;
		indices[c++] = 2;
		indices[c++] = 2;
		indices[c++] = 3;
		indices[c++] = 3;
		indices[c++] = 0;

		indices[c++] = 4;
		indices[c++] = 5;
		indices[c++] = 5;
		indices[c++] = 6;
		indices[c++] = 6;
		indices[c++] = 7;
		indices[c++] = 7;
		indices[c++] = 4;

		indices[c++] = 0;
		indices[c++] = 4;
		indices[c++] = 1;
		indices[c++] = 5;
		indices[c++] = 2;
		indices[c++] = 6;
		indices[c++] = 3;
		indices[c++] = 7;

		m_cubeIndicesBuffer->unmap();

		ANKI_ASSERT(c == kIndexCount);
	}

	return Error::kNone;
}

void Dbg::drawNonRenderable(GpuSceneNonRenderableObjectType type, U32 objCount, const RenderingContext& ctx, const ImageResource& image,
							CommandBuffer& cmdb)
{
	ShaderProgramResourceVariantInitInfo variantInitInfo(m_nonRenderablesProg);
	variantInitInfo.addMutation("DEPTH_FAIL_VISUALIZATION", U32(m_ditheredDepthTestOn != 0));
	variantInitInfo.addMutation("OBJECT_TYPE", U32(type));
	const ShaderProgramResourceVariant* variant;
	m_nonRenderablesProg->getOrCreateVariant(variantInitInfo, variant);
	cmdb.bindShaderProgram(&variant->getProgram());

	class Uniforms
	{
	public:
		Mat4 m_viewProjMat;
		Mat3x4 m_camTrf;
	} unis;
	unis.m_viewProjMat = ctx.m_matrices.m_viewProjection;
	unis.m_camTrf = ctx.m_matrices.m_cameraTransform;
	cmdb.setPushConstants(&unis, sizeof(unis));

	cmdb.bindStorageBuffer(ANKI_REG(t1), getRenderer().getClusterBinning().getPackedObjectsBuffer(type));
	cmdb.bindStorageBuffer(ANKI_REG(t2), getRenderer().getPrimaryNonRenderableVisibility().getVisibleIndicesBuffer(type));

	cmdb.bindSampler(ANKI_REG(s1), getRenderer().getSamplers().m_trilinearRepeat.get());
	cmdb.bindTexture(ANKI_REG(t3), TextureView(&image.getTexture(), TextureSubresourceDesc::all()));
	cmdb.bindTexture(ANKI_REG(t4), TextureView(&m_spotLightImage->getTexture(), TextureSubresourceDesc::all()));

	cmdb.draw(PrimitiveTopology::kTriangles, 6, objCount);
}

void Dbg::run(RenderPassWorkContext& rgraphCtx, const RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(Dbg);
	ANKI_ASSERT(g_dbgCVar.get());

	CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

	// Set common state
	cmdb.setViewport(0, 0, getRenderer().getInternalResolution().x(), getRenderer().getInternalResolution().y());
	cmdb.setDepthWrite(false);

	cmdb.setBlendFactors(0, BlendFactor::kSrcAlpha, BlendFactor::kOneMinusSrcAlpha);
	cmdb.setDepthCompareOperation((m_depthTestOn) ? CompareOperation::kLess : CompareOperation::kAlways);
	cmdb.setLineWidth(2.0f);

	cmdb.bindSampler(ANKI_REG(s0), getRenderer().getSamplers().m_nearestNearestClamp.get());
	rgraphCtx.bindTexture(ANKI_REG(t0), getRenderer().getGBuffer().getDepthRt());

	// GBuffer renderables
	{
		const U32 allAabbCount = GpuSceneArrays::RenderableBoundingVolumeGBuffer::getSingleton().getElementCount();

		ShaderProgramResourceVariantInitInfo variantInitInfo(m_renderablesProg);
		variantInitInfo.addMutation("DEPTH_FAIL_VISUALIZATION", U32(m_ditheredDepthTestOn != 0));
		const ShaderProgramResourceVariant* variant;
		m_renderablesProg->getOrCreateVariant(variantInitInfo, variant);
		cmdb.bindShaderProgram(&variant->getProgram());

		class Uniforms
		{
		public:
			Vec4 m_color;
			Mat4 m_viewProjMat;
		} unis;
		unis.m_color = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
		unis.m_viewProjMat = ctx.m_matrices.m_viewProjection;

		cmdb.setPushConstants(&unis, sizeof(unis));
		cmdb.bindVertexBuffer(0, BufferView(m_cubeVertsBuffer.get()), sizeof(Vec3));
		cmdb.setVertexAttribute(VertexAttributeSemantic::kPosition, 0, Format::kR32G32B32_Sfloat, 0);
		cmdb.bindIndexBuffer(BufferView(m_cubeIndicesBuffer.get()), IndexType::kU16);

		cmdb.bindStorageBuffer(ANKI_REG(t1), GpuSceneArrays::RenderableBoundingVolumeGBuffer::getSingleton().getBufferView());

		BufferView indicesBuff;
		BufferHandle dep;
		getRenderer().getGBuffer().getVisibleAabbsBuffer(indicesBuff, dep);
		cmdb.bindStorageBuffer(ANKI_REG(t2), indicesBuff);

		cmdb.drawIndexed(PrimitiveTopology::kLines, 12 * 2, allAabbCount);
	}

	// Forward shading renderables
	{
		const U32 allAabbCount = GpuSceneArrays::RenderableBoundingVolumeForward::getSingleton().getElementCount();

		if(allAabbCount)
		{
			cmdb.bindStorageBuffer(ANKI_REG(t1), GpuSceneArrays::RenderableBoundingVolumeForward::getSingleton().getBufferView());

			BufferView indicesBuff;
			BufferHandle dep;
			getRenderer().getForwardShading().getVisibleAabbsBuffer(indicesBuff, dep);
			cmdb.bindStorageBuffer(ANKI_REG(t2), indicesBuff);

			cmdb.drawIndexed(PrimitiveTopology::kLines, 12 * 2, allAabbCount);
		}
	}

	// Draw non-renderables
	drawNonRenderable(GpuSceneNonRenderableObjectType::kLight, GpuSceneArrays::Light::getSingleton().getElementCount(), ctx, *m_pointLightImage,
					  cmdb);
	drawNonRenderable(GpuSceneNonRenderableObjectType::kDecal, GpuSceneArrays::Decal::getSingleton().getElementCount(), ctx, *m_decalImage, cmdb);
	drawNonRenderable(GpuSceneNonRenderableObjectType::kGlobalIlluminationProbe,
					  GpuSceneArrays::GlobalIlluminationProbe::getSingleton().getElementCount(), ctx, *m_giProbeImage, cmdb);
	drawNonRenderable(GpuSceneNonRenderableObjectType::kReflectionProbe, GpuSceneArrays::ReflectionProbe::getSingleton().getElementCount(), ctx,
					  *m_reflectionImage, cmdb);

	// Restore state
	cmdb.setDepthCompareOperation(CompareOperation::kLess);
}

void Dbg::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(Dbg);

	if(!g_dbgCVar.get())
	{
		return;
	}

	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	// Create RT
	m_runCtx.m_rt = rgraph.newRenderTarget(m_rtDescr);

	// Create pass
	GraphicsRenderPass& pass = rgraph.newGraphicsRenderPass("Debug");

	pass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
		run(rgraphCtx, ctx);
	});

	GraphicsRenderPassTargetDesc colorRti(m_runCtx.m_rt);
	colorRti.m_loadOperation = RenderTargetLoadOperation::kClear;
	GraphicsRenderPassTargetDesc depthRti(getRenderer().getGBuffer().getDepthRt());
	depthRti.m_loadOperation = RenderTargetLoadOperation::kLoad;
	pass.setRenderpassInfo({colorRti}, &depthRti);

	pass.newTextureDependency(m_runCtx.m_rt, TextureUsageBit::kRtvDsvWrite);
	pass.newTextureDependency(getRenderer().getGBuffer().getDepthRt(), TextureUsageBit::kSrvFragment | TextureUsageBit::kRtvDsvRead);

	BufferView indicesBuff;
	BufferHandle dep;
	getRenderer().getGBuffer().getVisibleAabbsBuffer(indicesBuff, dep);
	pass.newBufferDependency(dep, BufferUsageBit::kSrvGeometry);

	if(GpuSceneArrays::RenderableBoundingVolumeForward::getSingleton().getElementCount())
	{
		getRenderer().getForwardShading().getVisibleAabbsBuffer(indicesBuff, dep);
		pass.newBufferDependency(dep, BufferUsageBit::kSrvGeometry);
	}
}

} // end namespace anki
