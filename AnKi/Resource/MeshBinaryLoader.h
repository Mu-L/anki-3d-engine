// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Resource/MeshBinary.h>
#include <AnKi/Resource/ResourceFilesystem.h>
#include <AnKi/Resource/MeshBinary.h>
#include <AnKi/Util/WeakArray.h>
#include <AnKi/Shaders/Include/MeshTypes.h>

namespace anki {

/// @addtogroup resource
/// @{

/// This class loads the mesh binary file. It only supports a subset of combinations of vertex formats and buffers.
/// The file is layed out in memory:
/// * Header
/// * Submeshes
/// * Index buffer max LOD
/// * Vertex stream 0 of max LOD
/// * etc...
class MeshBinaryLoader
{
public:
	MeshBinaryLoader(ResourceManager* manager);

	MeshBinaryLoader(ResourceManager* manager, GenericMemoryPoolAllocator<U8> alloc)
		: m_manager(manager)
		, m_alloc(alloc)
	{
	}

	~MeshBinaryLoader();

	Error load(const ResourceFilename& filename);

	Error storeIndexBuffer(U32 lod, void* ptr, PtrSize size);

	Error storeVertexBuffer(U32 lod, U32 bufferIdx, void* ptr, PtrSize size);

	/// Instead of calling storeIndexBuffer and storeVertexBuffer use this method to get those buffers into the CPU.
	Error storeIndicesAndPosition(U32 lod, DynamicArrayAuto<U32>& indices, DynamicArrayAuto<Vec3>& positions);

	const MeshBinaryHeader& getHeader() const
	{
		ANKI_ASSERT(isLoaded());
		return m_header;
	}

	ConstWeakArray<MeshBinarySubMesh> getSubMeshes() const
	{
		return ConstWeakArray<MeshBinarySubMesh>(m_subMeshes);
	}

private:
	ResourceManager* m_manager;
	GenericMemoryPoolAllocator<U8> m_alloc;
	ResourceFilePtr m_file;

	MeshBinaryHeader m_header;

	DynamicArray<MeshBinarySubMesh> m_subMeshes;

	Bool isLoaded() const
	{
		return m_file.get() != nullptr;
	}

	PtrSize getIndexBufferSize(U32 lod) const
	{
		ANKI_ASSERT(isLoaded());
		ANKI_ASSERT(lod < m_header.m_lodCount);
		return PtrSize(m_header.m_totalIndexCounts[lod]) * getIndexSize(m_header.m_indexType);
	}

	PtrSize getVertexBufferSize(U32 lod, U32 bufferIdx) const
	{
		ANKI_ASSERT(isLoaded());
		ANKI_ASSERT(lod < m_header.m_lodCount);
		ANKI_ASSERT(bufferIdx < m_header.m_vertexBufferCount);
		return PtrSize(m_header.m_totalVertexCounts[lod]) * PtrSize(m_header.m_vertexBuffers[bufferIdx].m_vertexStride);
	}

	PtrSize getLodBuffersSize(U32 lod) const;

	Error checkHeader() const;
	Error checkFormat(VertexStreamId stream, ConstWeakArray<Format> supportedFormats) const;
	Error loadSubmeshes();
};
/// @}

} // end namespace anki
