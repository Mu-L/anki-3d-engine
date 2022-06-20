// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Shaders/Include/MeshTypes.h>

ANKI_BEGIN_NAMESPACE

struct LodInfo
{
	U32 m_baseVertex[MAX_VERTEX_STREAM_COUNT];
	U32 m_baseIndex;
	U32 m_indexCount;
};
ANKI_SHADER_STATIC_ASSERT(sizeof(LodInfo) == sizeof(Vec4) * 2u);

struct RenderableGpuView
{
	Mat3x4 m_worldTransform;
	Mat3x4 m_previousWorldTransform;

	Vec3 m_aabbMin;
	U32 m_lodCount;

	Vec3 m_aabbMax;
	U32 m_localUniformsOffset;

	LodInfo m_lods[MAX_LOD_COUNT];
};
ANKI_SHADER_STATIC_ASSERT(sizeof(RenderableGpuView) == sizeof(Vec4) * 14u);

struct SkinGpuView
{
	U32 m_tmp;
};

ANKI_END_NAMESPACE
