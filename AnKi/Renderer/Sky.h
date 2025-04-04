// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Renderer/RendererObject.h>
#include <AnKi/Resource/ImageResource.h>
#include <AnKi/Gr.h>

namespace anki {

/// @addtogroup renderer
/// @{

/// Resolves shadowmaps into a single texture.
class GeneratedSky : public RendererObject
{
public:
	GeneratedSky()
	{
		registerDebugRenderTarget("SkyLut");
	}

	Error init();

	void populateRenderGraph(RenderingContext& ctx);

	void getDebugRenderTarget([[maybe_unused]] CString rtName, Array<RenderTargetHandle, kMaxDebugRenderTargets>& handles,
							  [[maybe_unused]] ShaderProgramPtr& optionalShaderProgram) const override
	{
		handles[0] = m_runCtx.m_envMapRt;
	}

	RenderTargetHandle getSkyLutRt() const
	{
		ANKI_ASSERT(isEnabled());
		return m_runCtx.m_skyLutRt;
	}

	RenderTargetHandle getEnvironmentMapRt() const
	{
		ANKI_ASSERT(isEnabled());
		return m_runCtx.m_envMapRt;
	}

	Texture& getEnvironmentMapTexture() const
	{
		return *m_envMap;
	}

	ANKI_PURE Bool isEnabled() const;

public:
	ShaderProgramResourcePtr m_prog;
	ShaderProgramPtr m_transmittanceLutGrProg;
	ShaderProgramPtr m_multipleScatteringLutGrProg;
	ShaderProgramPtr m_skyLutGrProg;
	ShaderProgramPtr m_computeSunColorGrProg;
	ShaderProgramPtr m_computeEnvMapGrProg;

	static constexpr UVec2 kTransmittanceLutSize{256, 64};
	static constexpr UVec2 kMultipleScatteringLutSize{32, 32};
	static constexpr UVec2 kSkyLutSize{256, 256};
	static constexpr UVec2 kEnvMapSize{64, 64};

	TexturePtr m_transmittanceLut;
	TexturePtr m_multipleScatteringLut;
	TexturePtr m_skyLut;
	TexturePtr m_envMap;

	Vec3 m_sunDir = Vec3(0.0f);
	F32 m_sunPower = -100.0f;

	Bool m_transmittanceAndMultiScatterLutsGenerated = false;
	Bool m_skyLutImportedOnce = false;

	class
	{
	public:
		RenderTargetHandle m_skyLutRt;
		RenderTargetHandle m_envMapRt;
	} m_runCtx;
};
/// @}

} // namespace anki
