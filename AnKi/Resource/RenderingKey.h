// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Resource/Common.h>
#include <AnKi/Util/Hash.h>

namespace anki {

/// The AnKi passes visible to materials.
enum class RenderingTechnique : U8
{
	kGBuffer = 0,
	kDepth = 1,
	kForward = 2,
	kRtShadow = 3,
	kRtMaterialFetch = 4,

	kCount,
	kFirst = 0,
	kFirstRt = kRtShadow,
	kLastRt = kRtMaterialFetch,
	kRtCount = kLastRt - kFirstRt + 1
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(RenderingTechnique)

enum class RenderingTechniqueBit : U8
{
	kNone = 0,
	kGBuffer = 1 << 0,
	kDepth = 1 << 1,
	kForward = 1 << 2,
	kRtShadow = 1 << 3,
	kRtMaterialFetch = 1 << 4,

	kAllRt = kRtShadow | kRtMaterialFetch,
	kAllRaster = kGBuffer | kDepth | kForward
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(RenderingTechniqueBit)

/// A key that consistst of the rendering pass and the level of detail
class RenderingKey
{
public:
	RenderingKey(RenderingTechnique technique, U32 lod, Bool skinned, Bool velocity, Bool meshletRendering)
		: m_technique(technique)
		, m_lod(lod & 0b11)
		, m_skinned(skinned)
		, m_velocity(velocity)
		, m_meshletRendering(meshletRendering)
	{
		ANKI_ASSERT(lod < kMaxLodCount);
	}

	RenderingKey()
		: RenderingKey(RenderingTechnique::kFirst, 0, false, false, false)
	{
	}

	RenderingKey(const RenderingKey& b)
		: RenderingKey(b.m_technique, b.m_lod, b.m_skinned, b.m_velocity, b.m_meshletRendering)
	{
	}

	RenderingKey& operator=(const RenderingKey& b)
	{
		memcpy(this, &b, sizeof(*this));
		return *this;
	}

	Bool operator==(const RenderingKey& b) const
	{
		return m_technique == b.m_technique && m_lod == b.m_lod && m_skinned == b.m_skinned && m_velocity == b.m_velocity
			   && m_meshletRendering == b.m_meshletRendering;
	}

	RenderingTechnique getRenderingTechnique() const
	{
		return m_technique;
	}

	void setRenderingTechnique(RenderingTechnique t)
	{
		m_technique = t;
	}

	U32 getLod() const
	{
		return m_lod;
	}

	void setLod(U32 lod)
	{
		ANKI_ASSERT(lod < kMaxLodCount);
		m_lod = lod & 0b11;
	}

	Bool getSkinned() const
	{
		return m_skinned;
	}

	void setSkinned(Bool is)
	{
		m_skinned = is;
	}

	Bool getVelocity() const
	{
		return m_velocity;
	}

	void setVelocity(Bool v)
	{
		m_velocity = v;
	}

	void setMeshletRendering(Bool b)
	{
		m_meshletRendering = b;
	}

	Bool getMeshletRendering() const
	{
		return m_meshletRendering;
	}

private:
	RenderingTechnique m_technique;
	U8 m_lod : 2;
	Bool m_skinned : 1;
	Bool m_velocity : 1;
	Bool m_meshletRendering : 1;

	static_assert(kMaxLodCount <= 3, "m_lod only reserves 2 bits so make sure all LODs will fit");
};

static_assert(sizeof(RenderingKey) == sizeof(U8) * 2, "RenderingKey needs to be packed because of hashing");

} // end namespace anki
