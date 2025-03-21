// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Shaders/Include/MiscRendererTypes.h>
#include <AnKi/Shaders/PackFunctions.hlsl>

constexpr F32 kRtShadowsMaxHistoryLength = 16.0; // The frames of history

struct [raypayload] RtShadowsRayPayload
{
	F32 m_shadowFactor : write(caller, anyhit, miss): read(caller);
};
