// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Renderer/RendererObject.h>

namespace anki {

/// @addtogroup renderer
/// @{

inline NumericCVar<F32>
	g_rayTracingExtendedFrustumDistanceCVar("R", "RayTracingExtendedFrustumDistance", 200.0f, 10.0f, 10000.0f,
											"Every object that its distance from the camera is bellow that value will take part in ray tracing");

/// Build acceleration structures.
class AccelerationStructureBuilder : public RendererObject
{
public:
	Error init()
	{
		return Error::kNone;
	}

	void populateRenderGraph(RenderingContext& ctx);

	AccelerationStructureHandle getAccelerationStructureHandle() const
	{
		return m_runCtx.m_tlasHandle;
	}

	void getVisibilityInfo(BufferHandle& depedency, BufferView& visibleRenderables, BufferView& buildSbtIndirectArgs) const
	{
		depedency = m_runCtx.m_dependency;
		visibleRenderables = m_runCtx.m_visibleRenderablesBuff;
		buildSbtIndirectArgs = m_runCtx.m_buildSbtIndirectArgsBuff;
	}

public:
	class
	{
	public:
		AccelerationStructurePtr m_tlas;
		AccelerationStructureHandle m_tlasHandle;

		BufferHandle m_dependency;
		BufferView m_visibleRenderablesBuff;
		BufferView m_buildSbtIndirectArgsBuff;
	} m_runCtx;
};
/// @}

} // namespace anki
