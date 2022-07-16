// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Resource/ResourceObject.h>
#include <AnKi/Math.h>
#include <AnKi/Gr.h>
#include <AnKi/Collision/Aabb.h>
#include <AnKi/Shaders/Include/MeshTypes.h>

namespace anki {

// Forward
class MeshBinaryLoader;

/// @addtogroup resource
/// @{

/// Mesh Resource. It contains the geometry packed in GPU buffers.
class MeshResource : public ResourceObject
{
public:
	/// Default constructor
	MeshResource(ResourceManager* manager);

	~MeshResource();

	/// Load from a mesh file
	Error load(const ResourceFilename& filename, Bool async);

	/// Get the complete bounding box.
	const Aabb& getBoundingShape() const
	{
		return m_aabb;
	}

	U32 getSubMeshCount() const
	{
		return m_subMeshes.getSize();
	}

	/// Get submesh info.
	void getSubMeshInfo(U32 lod, U32 subMeshId, U32& firstIndex, U32& indexCount, Aabb& aabb) const
	{
		const SubMesh& sm = m_subMeshes[subMeshId];
		firstIndex = sm.m_firstIndices[lod];
		indexCount = sm.m_indexCounts[lod];
		aabb = sm.m_aabb;
	}

	/// Get all info around vertex indices.
	void getIndexBufferInfo(U32 lod, PtrSize& buffOffset, U32& indexCount, IndexType& indexType) const
	{
		buffOffset = m_lods[lod].m_globalIndexBufferOffset;
		ANKI_ASSERT(isAligned(getIndexSize(m_indexType), buffOffset));
		indexCount = m_lods[lod].m_indexCount;
		indexType = m_indexType;
	}

	/// Get vertex buffer info.
	void getVertexStreamInfo(U32 lod, VertexStreamId stream, PtrSize& bufferOffset, U32& vertexCount) const
	{
		bufferOffset = m_lods[lod].m_globalVertexBufferOffsets[stream];
		vertexCount = m_lods[lod].m_vertexCount;
	}

	const AccelerationStructurePtr& getBottomLevelAccelerationStructure(U32 lod) const
	{
		ANKI_ASSERT(m_lods[lod].m_blas);
		return m_lods[lod].m_blas;
	}

	/// Check if a vertex stream is present.
	Bool isVertexStreamPresent(const VertexStreamId stream) const
	{
		return !!VertexStreamBit(1 << stream);
	}

	Bool getLodCount() const
	{
		return m_lods.getSize();
	}

private:
	class LoadTask;
	class LoadContext;

	class Lod
	{
	public:
		PtrSize m_globalIndexBufferOffset = MAX_PTR_SIZE;
		Array<PtrSize, U32(VertexStreamId::COUNT)> m_globalVertexBufferOffsets;

		U32 m_indexCount = 0;
		U32 m_vertexCount = 0;

		AccelerationStructurePtr m_blas;

		Lod()
		{
			m_globalVertexBufferOffsets.fill(m_globalVertexBufferOffsets.getBegin(),
											 m_globalVertexBufferOffsets.getEnd(), MAX_PTR_SIZE);
		}
	};

	class SubMesh
	{
	public:
		Array<U32, MAX_LOD_COUNT> m_firstIndices;
		Array<U32, MAX_LOD_COUNT> m_indexCounts;
		Aabb m_aabb;
	};

	DynamicArray<SubMesh> m_subMeshes;
	DynamicArray<Lod> m_lods;
	Aabb m_aabb;
	IndexType m_indexType;
	VertexStreamBit m_presentVertStreams = VertexStreamBit::NONE;

	Error loadAsync(MeshBinaryLoader& loader) const;
};
/// @}

} // end namespace anki
