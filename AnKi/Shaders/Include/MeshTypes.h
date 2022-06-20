// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Shaders/Include/Common.h>

ANKI_BEGIN_NAMESPACE

#if defined(__cplusplus)
enum class VertexStreamId : U8
{
	FIRST = 0u,
	COUNT = 6u,

	// Aliases for regular meshes
	POSITIONS = 0u,
	NORMALS = 1u,
	TANGENTS = 2u,
	UVS = 3u,
	BONE_IDS = 4u,
	BONE_WEIGHTS = 5u,

	// Aliases for particles
	SCALES = 0u,
	ALPHAS = 1u,

	// Aliases for GPU particles
	PREVIOUS_POSITIONS = 1u,
	LIFE = 2u,
	STARTING_LIFE = 3u,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(VertexStreamId)

enum class VertexStreamBit : U8
{
	NONE = 0,

	// Aliases for regular meshes
	POSITIONS = U8(1 << VertexStreamId::POSITIONS),
	NORMALS = U8(1 << VertexStreamId::NORMALS),
	TANGENTS = U8(1 << VertexStreamId::TANGENTS),
	UVS = U8(1 << VertexStreamId::UVS),
	BONE_IDS = U8(1 << VertexStreamId::BONE_IDS),
	BONE_WEIGHTS = U8(1 << VertexStreamId::BONE_WEIGHTS),

	// Aliases for particles
	SCALES = U8(1 << VertexStreamId::SCALES),
	ALPHAS = U8(1 << VertexStreamId::ALPHAS),

	// Aliases for GPU particles
	PREVIOUS_POSITIONS = U8(1 << VertexStreamId::PREVIOUS_POSITIONS),
	LIFE = U8(1 << VertexStreamId::LIFE),
	STARTING_LIFE = U8(1 << VertexStreamId::STARTING_LIFE),
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(VertexStreamBit)

/// Formats for the vertex streams of a regular mesh.
static constexpr Array<Format, U32(VertexStreamId::BONE_WEIGHTS) + 1> REGULAR_VERTEX_STREAM_FORMATS = {
	Format::R32G32B32_SFLOAT, Format::R8G8B8A8_SNORM, Format::R8G8B8A8_SNORM,
	Format::R32G32_SFLOAT,    Format::R8G8B8A8_UINT,  Format::R8G8B8A8_SNORM};
#else
const U32 MAX_VERTEX_STREAM_COUNT = 6u;

// Aliases for regular meshes
const U32 VERTEX_STREAM_POSITIONS = 0u;
const U32 VERTEX_STREAM_NORMALS = 1u;
const U32 VERTEX_STREAM_TANGENTS = 2u;
const U32 VERTEX_STREAM_UVS = 3u;
const U32 VERTEX_STREAM_BONE_IDS = 4u;
const U32 VERTEX_STREAM_BONE_WEIGHTS = 5u;

// Aliases for particles
const U32 VERTEX_STREAM_SCALES = 0u;
const U32 VERTEX_STREAM_ALPHAS = 1u;

// Aliases for GPU particles
const U32 VERTEX_STREAM_PREVIOUS_POSITIONS = 1u;
const U32 VERTEX_STREAM_LIFE = 2u;
const U32 VERTEX_STREAM_STARTING_LIFE = 3u;
#endif

ANKI_END_NAMESPACE
