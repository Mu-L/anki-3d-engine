// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Gr/Common.h>
#include <AnKi/Util/Atomic.h>

namespace anki {

/// @addtogroup graphics
/// @{

/// Graphics object type.
enum class GrObjectType : U8
{
	kBuffer,
	kCommandBuffer,
	kFramebuffer,
	kOcclusionQuery,
	kTimstampQuery,
	kPipelineQuery,
	kSampler,
	kShader,
	kTexture,
	kShaderProgram,
	kFence,
	kRenderGraph,
	kAccelerationStructure,
	kGrUpscaler,

	kCount,
	kFirst = 0
};

/// Base of all graphics objects.
class GrObject
{
public:
	GrObject(GrObjectType type, CString name);

	GrObject(const GrObject&) = delete; // Non-copyable

	virtual ~GrObject();

	GrObject& operator=(const GrObject&) = delete; // Non-copyable

	GrObjectType getType() const
	{
		return m_type;
	}

	void retain() const
	{
		m_refcount.fetchAdd(1);
	}

	I32 release() const
	{
		return m_refcount.fetchSub(1);
	}

	/// A unique identifier for caching objects.
	U64 getUuid() const
	{
		return m_uuid;
	}

	/// Get its name.
	CString getName() const
	{
		return m_name;
	}

private:
	Char* m_name = nullptr;
	U64 m_uuid;
	mutable Atomic<I32> m_refcount;
	GrObjectType m_type;
};
/// @}

} // end namespace anki
