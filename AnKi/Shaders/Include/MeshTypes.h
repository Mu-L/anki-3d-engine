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
	POSITION = 0u,
	NORMAL = 1u,
	TANGENT = 2u,
	UV = 3u,
	BONE_IDS = 4u,
	BONE_WEIGHTS = 5u,

	// Aliases for particles
	PARTICLE_SCALE = 1u,
	PARTICLE_ALPHA = 2u,
	PARTICLE_LIFE = 3u,
	PARTICLE_STARTING_LIFE = 4u,
	PARTICLE_PREVIOUS_POSITION = 5u,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(VertexStreamId)

enum class VertexStreamBit : U8
{
	NONE = 0,

	// Aliases for regular meshes
	POSITION = U8(1 << VertexStreamId::POSITION),
	NORMAL = U8(1 << VertexStreamId::NORMAL),
	TANGENT = U8(1 << VertexStreamId::TANGENT),
	UV = U8(1 << VertexStreamId::UV),
	BONE_IDS = U8(1 << VertexStreamId::BONE_IDS),
	BONE_WEIGHTS = U8(1 << VertexStreamId::BONE_WEIGHTS),

	// Aliases for particles
	PARTICLE_SCALE = U8(1 << VertexStreamId::PARTICLE_SCALE),
	PARTICLE_ALPHA = U8(1 << VertexStreamId::PARTICLE_ALPHA),
	PARTICLE_LIFE = U8(1 << VertexStreamId::PARTICLE_LIFE),
	PARTICLE_STARTING_LIFE = U8(1 << VertexStreamId::PARTICLE_STARTING_LIFE),
	PARTICLE_PREVIOUS_POSITION = U8(1 << VertexStreamId::PARTICLE_PREVIOUS_POSITION),
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(VertexStreamBit)

/// Formats for the vertex streams of a regular mesh.
static constexpr Array<Format, U32(VertexStreamId::BONE_WEIGHTS) + 1> REGULAR_VERTEX_STREAM_FORMATS = {
	Format::R32G32B32_SFLOAT, Format::R8G8B8A8_SNORM, Format::R8G8B8A8_SNORM,
	Format::R32G32_SFLOAT,    Format::R8G8B8A8_UINT,  Format::R8G8B8A8_SNORM};
#else
const U32 MAX_VERTEX_STREAM_COUNT = 6u;

const U32 VERTEX_STREAM_POSITION = 0u;
const U32 VERTEX_STREAM_NORMAL = 1u;
const U32 VERTEX_STREAM_TANGENT = 2u;
const U32 VERTEX_STREAM_UV = 3u;
const U32 VERTEX_STREAM_BONE_IDS = 4u;
const U32 VERTEX_STREAM_BONE_WEIGHTS = 5u;

const U32 VERTEX_STREAM_PARTICLE_SCALE = 1u;
const U32 VERTEX_STREAM_PARTICLE_ALPHA = 2u;
const U32 VERTEX_STREAM_PARTICLE_LIFE = 3u;
const U32 VERTEX_STREAM_PARTICLE_STARTING_LIFE = 4u;
const U32 VERTEX_STREAM_PARTICLE_PREVIOUS_POSITION = 5u;
#endif

ANKI_END_NAMESPACE
