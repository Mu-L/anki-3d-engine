// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Renderer/RendererObject.h>

namespace anki {

/// @addtogroup renderer
/// @{

inline NumericCVar<U8> g_fsrQualityCVar("R", "FsrQuality", 1, 0, 2, "0: Use bilinear, 1: FSR low quality, 2: FSR high quality");
inline NumericCVar<U8> g_dlssQualityCVar("R", "DlssQuality", 2, 0, 3, "0: Disabled, 1: Performance, 2: Balanced, 3: Quality");
inline NumericCVar<F32> g_sharpnessCVar("R", "Sharpness", (ANKI_PLATFORM_MOBILE) ? 0.0f : 0.8f, 0.0f, 1.0f, "Sharpen the image. It's a factor");

/// Upscales, sharpens and in some cases tonemaps.
class Scale : public RendererObject
{
public:
	Error init();

	void populateRenderGraph(RenderingContext& ctx);

	/// This is the tonemapped, upscaled and sharpened RT.
	RenderTargetHandle getTonemappedRt() const
	{
		return m_runCtx.m_sharpenedRt;
	}

	/// This is the HDR upscaled RT. It's available if hasUscaledHdrRt() returns true.
	RenderTargetHandle getUpscaledHdrRt() const
	{
		ANKI_ASSERT(hasUpscaledHdrRt());
		return m_runCtx.m_upscaledHdrRt;
	}

	/// @see getUpscaledHdrRt.
	Bool hasUpscaledHdrRt() const
	{
		return m_runCtx.m_upscaledHdrRt.isValid();
	}

	Bool getUsingGrUpscaler() const
	{
		return m_grUpscaler.isCreated();
	}

private:
	ShaderProgramResourcePtr m_scaleProg;
	ShaderProgramPtr m_scaleGrProg;
	ShaderProgramResourcePtr m_sharpenProg;
	ShaderProgramPtr m_sharpenGrProg;
	ShaderProgramResourcePtr m_tonemapProg;
	ShaderProgramPtr m_tonemapGrProg;

	GrUpscalerPtr m_grUpscaler;

	RenderTargetDesc m_upscaleAndSharpenRtDescr;
	RenderTargetDesc m_tonemapedRtDescr;

	enum class UpscalingMethod : U8
	{
		kNone,
		kBilinear,
		kFsr,
		kGr,
		kCount
	};

	UpscalingMethod m_upscalingMethod = UpscalingMethod::kNone;

	enum class SharpenMethod : U8
	{
		kNone,
		kRcas,
		kCount
	};

	SharpenMethod m_sharpenMethod = SharpenMethod::kNone;

	Bool m_neeedsTonemapping = false;

	class
	{
	public:
		RenderTargetHandle m_upscaledTonemappedRt;
		RenderTargetHandle m_upscaledHdrRt;
		RenderTargetHandle m_tonemappedRt;
		RenderTargetHandle m_sharpenedRt; ///< It's tonemaped.
	} m_runCtx;

	void runFsrOrBilinearScaling(RenderPassWorkContext& rgraphCtx);
	void runRcasSharpening(RenderPassWorkContext& rgraphCtx);
	void runGrUpscaling(RenderingContext& ctx, RenderPassWorkContext& rgraphCtx);
	void runTonemapping(RenderPassWorkContext& rgraphCtx);
};
/// @}

} // end namespace anki
