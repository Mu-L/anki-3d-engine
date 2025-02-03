// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Resource/ResourceObject.h>
#include <AnKi/Math.h>
#include <AnKi/Util/WeakArray.h>
#include <AnKi/Physics/PhysicsCollisionShape.h>

namespace anki {

/// @addtogroup resource
/// @{

/// CPU Mesh Resource. It contains the geometry packed in CPU buffers.
class CpuMeshResource : public ResourceObject
{
public:
	/// Default constructor
	CpuMeshResource() = default;

	~CpuMeshResource() = default;

	/// Load from a mesh file
	Error load(const ResourceFilename& filename, Bool async);

	ConstWeakArray<Vec3> getPositions() const
	{
		return m_positions;
	}

	ConstWeakArray<U32> getIndices() const
	{
		return m_indices;
	}

	const PhysicsCollisionShapePtr& getOrCreateCollisionShape(Bool isStatic) const;

private:
	ResourceDynamicArray<Vec3> m_positions;
	ResourceDynamicArray<U32> m_indices;

	mutable PhysicsCollisionShapePtr m_collisionShape;
	mutable SpinLock m_shapeMtx;

	Bool m_isConvex = false;
};
/// @}

} // namespace anki
