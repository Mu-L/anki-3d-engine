// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/PrimaryNonRenderableVisibility.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/Utils/GpuVisibility.h>
#include <AnKi/Shaders/Include/GpuSceneFunctions.h>
#include <AnKi/Scene/GpuSceneArray.h>
#include <AnKi/Scene/Components/LightComponent.h>
#include <AnKi/Scene/Components/ReflectionProbeComponent.h>
#include <AnKi/Scene/Components/GlobalIlluminationProbeComponent.h>
#include <AnKi/Util/Tracer.h>

namespace anki {

template<typename TComponent, typename TArray, typename TPool>
static WeakArray<TComponent*> gatherComponents(ConstWeakArray<UVec2> pairs, TArray& array, TPool& pool)
{
	DynamicArray<TComponent*, MemoryPoolPtrWrapper<StackMemoryPool>> components(&pool);

	for(UVec2 pair : pairs)
	{
		if(!array.indexExists(pair.y()))
		{
			continue;
		}

		TComponent* comp = &array[pair.y()];
		if(comp->getUuid() == pair.x())
		{
			components.emplaceBack(comp);
		}
	}

	WeakArray<TComponent*> out;
	components.moveAndReset(out);
	return out;
}

void PrimaryNonRenderableVisibility::populateRenderGraph(RenderingContext& ctx)
{
	ANKI_TRACE_SCOPED_EVENT(PrimaryNonRenderableVisibility);
	RenderGraphBuilder& rgraph = ctx.m_renderGraphDescr;

	m_runCtx = {};

	for(GpuSceneNonRenderableObjectType type : EnumIterable<GpuSceneNonRenderableObjectType>())
	{
		U32 objCount = 0;
		CString passName;
		switch(type)
		{
		case GpuSceneNonRenderableObjectType::kLight:
			objCount = GpuSceneArrays::Light::getSingleton().getElementCount();
			passName = "Primary non-renderable visibility: Lights";
			break;
		case GpuSceneNonRenderableObjectType::kDecal:
			objCount = GpuSceneArrays::Decal::getSingleton().getElementCount();
			passName = "Primary non-renderable visibility: Decals";
			break;
		case GpuSceneNonRenderableObjectType::kFogDensityVolume:
			objCount = GpuSceneArrays::FogDensityVolume::getSingleton().getElementCount();
			passName = "Primary non-renderable visibility: Fog volumes";
			break;
		case GpuSceneNonRenderableObjectType::kReflectionProbe:
			objCount = GpuSceneArrays::ReflectionProbe::getSingleton().getElementCount();
			passName = "Primary non-renderable visibility: Refl probes";
			break;
		case GpuSceneNonRenderableObjectType::kGlobalIlluminationProbe:
			objCount = GpuSceneArrays::GlobalIlluminationProbe::getSingleton().getElementCount();
			passName = "Primary non-renderable visibility: GI probes";
			break;
		default:
			ANKI_ASSERT(0);
		}

		if(objCount == 0)
		{
			// No objects, point to a buffer with zeros

			WeakArray<U32> mem;
			const BufferView alloc = RebarTransientMemoryPool::getSingleton().allocateStructuredBuffer(1, mem);
			mem[0] = 0;

			m_runCtx.m_visibleIndicesBuffers[type] = alloc;
			m_runCtx.m_visibleIndicesHandles[type] = rgraph.importBuffer(m_runCtx.m_visibleIndicesBuffers[type], BufferUsageBit::kNone);
		}
		else
		{
			// Some objects, perform visibility testing

			GpuVisibilityNonRenderablesInput in;
			in.m_passesName = passName;
			in.m_objectType = type;
			in.m_viewProjectionMat = ctx.m_matrices.m_viewProjection;
			in.m_hzbRt = &getGBuffer().getHzbRt();
			in.m_rgraph = &rgraph;

			const GpuSceneNonRenderableObjectTypeWithFeedback feedbackType = toGpuSceneNonRenderableObjectTypeWithFeedback(type);
			if(feedbackType != GpuSceneNonRenderableObjectTypeWithFeedback::kCount)
			{
				// Read feedback from the GPU
				DynamicArray<U32, MemoryPoolPtrWrapper<StackMemoryPool>> readbackData(&getRenderer().getFrameMemoryPool());
				getRenderer().getReadbackManager().readMostRecentData(m_readbacks[feedbackType], readbackData);

				if(readbackData.getSize())
				{
					ANKI_ASSERT(readbackData.getSize() > 1);
					const U32 pairCount = readbackData[0];

					if(pairCount)
					{
						WeakArray<UVec2> pairs(reinterpret_cast<UVec2*>(&readbackData[1]), pairCount);
						if(feedbackType == GpuSceneNonRenderableObjectTypeWithFeedback::kLight)
						{
							m_runCtx.m_interestingComponents.m_shadowLights = gatherComponents<LightComponent>(
								pairs, SceneGraph::getSingleton().getComponentArrays().getLights(), getRenderer().getFrameMemoryPool());
						}
						else if(feedbackType == GpuSceneNonRenderableObjectTypeWithFeedback::kReflectionProbe)
						{
							m_runCtx.m_interestingComponents.m_reflectionProbes = gatherComponents<ReflectionProbeComponent>(
								pairs, SceneGraph::getSingleton().getComponentArrays().getReflectionProbes(), getRenderer().getFrameMemoryPool());
						}
						else
						{
							ANKI_ASSERT(feedbackType == GpuSceneNonRenderableObjectTypeWithFeedback::kGlobalIlluminationProbe);
							m_runCtx.m_interestingComponents.m_globalIlluminationProbes = gatherComponents<GlobalIlluminationProbeComponent>(
								pairs, SceneGraph::getSingleton().getComponentArrays().getGlobalIlluminationProbes(),
								getRenderer().getFrameMemoryPool());
						}
					}
				}

				// Allocate feedback buffer for this frame
				in.m_cpuFeedbackBuffer =
					getRenderer().getReadbackManager().allocateStructuredBuffer<U32>(m_readbacks[feedbackType], objCount * 2 + 1);
			}

			GpuVisibilityNonRenderablesOutput out;
			getRenderer().getGpuVisibilityNonRenderables().populateRenderGraph(in, out);

			m_runCtx.m_visibleIndicesHandles[type] = out.m_visiblesBufferHandle;
			m_runCtx.m_visibleIndicesBuffers[type] = out.m_visiblesBuffer;
		}
	}
}

} // end namespace anki
