// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Resource/MeshBinaryLoader.h>
#include <AnKi/Resource/ResourceManager.h>

namespace anki {

MeshBinaryLoader::MeshBinaryLoader(ResourceManager* manager)
	: MeshBinaryLoader(manager, manager->getTempAllocator())
{
}

MeshBinaryLoader::~MeshBinaryLoader()
{
	m_subMeshes.destroy(m_alloc);
}

Error MeshBinaryLoader::load(const ResourceFilename& filename)
{
	// Load header + submeshes
	ANKI_CHECK(m_manager->getFilesystem().openFile(filename, m_file));
	ANKI_CHECK(m_file->read(&m_header, sizeof(m_header)));
	ANKI_CHECK(checkHeader());
	ANKI_CHECK(loadSubmeshes());

	return Error::NONE;
}

Error MeshBinaryLoader::loadSubmeshes()
{
	m_subMeshes.create(m_alloc, m_header.m_subMeshCount);
	ANKI_CHECK(m_file->read(&m_subMeshes[0], m_subMeshes.getSizeInBytes()));

	// Checks
	const U32 indicesPerFace = !!(m_header.m_flags & MeshBinaryFlag::QUAD) ? 4 : 3;

	for(U32 lod = 0; lod < m_header.m_lodCount; ++lod)
	{
		U idxSum = 0;
		for(U32 i = 0; i < m_subMeshes.getSize(); i++)
		{
			const MeshBinarySubMesh& sm = m_subMeshes[i];
			if(sm.m_firstIndices[lod] != idxSum || (sm.m_indexCounts[lod] % indicesPerFace) != 0)
			{
				ANKI_RESOURCE_LOGE("Incorrect sub mesh info");
				return Error::USER_DATA;
			}

			for(U d = 0; d < 3; ++d)
			{
				if(sm.m_aabbMin[d] >= sm.m_aabbMax[d])
				{
					ANKI_RESOURCE_LOGE("Wrong submesh bounding box");
					return Error::USER_DATA;
				}
			}

			idxSum += sm.m_indexCounts[lod];
		}

		if(idxSum != m_header.m_totalIndexCounts[lod])
		{
			ANKI_RESOURCE_LOGE("Submesh index cound doesn't add up to the total");
			return Error::USER_DATA;
		}
	}

	return Error::NONE;
}

Error MeshBinaryLoader::checkFormat(VertexStreamId stream, ConstWeakArray<Format> supportedFormats) const
{
	const U32 vertexAttribIdx = U32(stream);
	const U32 vertexBufferIdx = vertexAttribIdx;
	const MeshBinaryVertexAttribute& attrib = m_header.m_vertexAttributes[vertexAttribIdx];

	// Check format
	Bool found = false;
	for(Format fmt : supportedFormats)
	{
		if(fmt == attrib.m_format)
		{
			found = true;
			break;
		}
	}

	if(!found)
	{
		ANKI_RESOURCE_LOGE("Vertex attribute %u has unsupported format %s", vertexAttribIdx,
						   getFormatInfo(attrib.m_format).m_name);
		return Error::USER_DATA;
	}

	if(!attrib.m_format)
	{
		// Attrib is not in use, no more checks
		return Error::NONE;
	}

	if(attrib.m_bufferBinding != vertexBufferIdx)
	{
		ANKI_RESOURCE_LOGE("Vertex attribute %u should belong to the %u vertex buffer", vertexAttribIdx,
						   vertexBufferIdx);
		return Error::USER_DATA;
	}

	if(attrib.m_relativeOffset != 0)
	{
		ANKI_RESOURCE_LOGE("Vertex attribute %u should have relative vertex offset equal to 0", vertexAttribIdx);
		return Error::USER_DATA;
	}

	// Scale should be 1.0 for now
	if(attrib.m_scale != 1.0f)
	{
		ANKI_RESOURCE_LOGE("Vertex attribute %u should have 1.0 scale", vertexAttribIdx);
		return Error::USER_DATA;
	}

	const U32 vertexBufferStride = getFormatInfo(attrib.m_format).m_texelSize;
	if(m_header.m_vertexBuffers[vertexBufferIdx].m_vertexStride != vertexBufferStride)
	{
		ANKI_RESOURCE_LOGE("Vertex buffer %u doesn't have the expected stride of %u", vertexBufferIdx,
						   vertexBufferStride);
		return Error::USER_DATA;
	}

	return Error::NONE;
}

Error MeshBinaryLoader::checkHeader() const
{
	const MeshBinaryHeader& h = m_header;

	// Header
	if(memcmp(&h.m_magic[0], MESH_MAGIC, 8) != 0)
	{
		ANKI_RESOURCE_LOGE("Wrong magic word");
		return Error::USER_DATA;
	}

	// Flags
	if((h.m_flags & ~MeshBinaryFlag::ALL) != MeshBinaryFlag::NONE)
	{
		ANKI_RESOURCE_LOGE("Wrong header flags");
		return Error::USER_DATA;
	}

	// Attributes
	ANKI_CHECK(checkFormat(VertexStreamId::POSITIONS,
						   Array<Format, 1>{{REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::POSITIONS]}}));
	ANKI_CHECK(checkFormat(VertexStreamId::NORMALS,
						   Array<Format, 1>{{REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::NORMALS]}}));
	ANKI_CHECK(checkFormat(VertexStreamId::TANGENTS,
						   Array<Format, 1>{{REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::TANGENTS]}}));
	ANKI_CHECK(
		checkFormat(VertexStreamId::UVS, Array<Format, 1>{{REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::UVS]}}));
	ANKI_CHECK(checkFormat(VertexStreamId::BONE_IDS,
						   Array<Format, 2>{{Format::NONE, REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::BONE_IDS]}}));
	ANKI_CHECK(
		checkFormat(VertexStreamId::BONE_WEIGHTS,
					Array<Format, 2>{{Format::NONE, REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::BONE_WEIGHTS]}}));

	// Vertex buffers
	const Format boneIdxFormat = m_header.m_vertexAttributes[VertexStreamId::BONE_IDS].m_format;
	const Format boneWeightsFormat = m_header.m_vertexAttributes[VertexStreamId::BONE_WEIGHTS].m_format;
	if((boneIdxFormat == Format::NONE && boneWeightsFormat != Format::NONE)
	   || (boneWeightsFormat == Format::NONE && boneIdxFormat != Format::NONE))
	{
		ANKI_RESOURCE_LOGE("Bone buffers are partially present");
		return Error::USER_DATA;
	}

	const Bool hasBoneBuffers = boneIdxFormat != Format::NONE && boneWeightsFormat != Format::NONE;

	if(m_header.m_vertexBufferCount != 4 + hasBoneBuffers * 2)
	{
		ANKI_RESOURCE_LOGE("Wrong number of vertex buffers");
		return Error::USER_DATA;
	}

	// LOD
	if(h.m_lodCount == 0 || h.m_lodCount >= MAX_LOD_COUNT)
	{
		ANKI_RESOURCE_LOGE("Wrong LOD count");
		return Error::USER_DATA;
	}

	// Indices format
	if(h.m_indexType != IndexType::U16)
	{
		ANKI_RESOURCE_LOGE("Wrong format for indices");
		return Error::USER_DATA;
	}

	// m_totalIndexCount
	for(U32 lod = 0; lod < h.m_lodCount; ++lod)
	{
		const U indicesPerFace = !!(h.m_flags & MeshBinaryFlag::QUAD) ? 4 : 3;
		if(h.m_totalIndexCounts[lod] == 0 || (h.m_totalIndexCounts[lod] % indicesPerFace) != 0)
		{
			ANKI_RESOURCE_LOGE("Wrong index count");
			return Error::USER_DATA;
		}
	}

	// m_totalVertexCount
	for(U32 lod = 0; lod < h.m_lodCount; ++lod)
	{
		if(h.m_totalVertexCounts[lod] == 0)
		{
			ANKI_RESOURCE_LOGE("Wrong vertex count");
			return Error::USER_DATA;
		}
	}

	// m_subMeshCount
	if(h.m_subMeshCount == 0)
	{
		ANKI_RESOURCE_LOGE("Wrong submesh count");
		return Error::USER_DATA;
	}

	// AABB
	for(U d = 0; d < 3; ++d)
	{
		if(h.m_aabbMin[d] >= h.m_aabbMax[d])
		{
			ANKI_RESOURCE_LOGE("Wrong bounding box");
			return Error::USER_DATA;
		}
	}

	// Check the file size
	PtrSize totalSize = sizeof(m_header);
	totalSize += sizeof(MeshBinarySubMesh) * m_header.m_subMeshCount;

	for(U32 lod = 0; lod < h.m_lodCount; ++lod)
	{
		totalSize += getLodBuffersSize(lod);
	}

	if(totalSize != m_file->getSize())
	{
		ANKI_RESOURCE_LOGE("Unexpected file size");
		return Error::USER_DATA;
	}

	return Error::NONE;
}

Error MeshBinaryLoader::storeIndexBuffer(U32 lod, void* ptr, PtrSize size)
{
	ANKI_ASSERT(ptr);
	ANKI_ASSERT(isLoaded());
	ANKI_ASSERT(lod < m_header.m_lodCount);
	ANKI_ASSERT(size == getIndexBufferSize(lod));

	PtrSize seek = sizeof(m_header) + m_subMeshes.getSizeInBytes();
	for(U32 l = lod + 1; l < m_header.m_lodCount; ++l)
	{
		seek += getLodBuffersSize(l);
	}

	ANKI_CHECK(m_file->seek(seek, FileSeekOrigin::BEGINNING));
	ANKI_CHECK(m_file->read(ptr, size));

	return Error::NONE;
}

Error MeshBinaryLoader::storeVertexBuffer(U32 lod, U32 bufferIdx, void* ptr, PtrSize size)
{
	ANKI_ASSERT(ptr);
	ANKI_ASSERT(isLoaded());
	ANKI_ASSERT(bufferIdx < m_header.m_vertexBufferCount);
	ANKI_ASSERT(size == getVertexBufferSize(lod, bufferIdx));
	ANKI_ASSERT(lod < m_header.m_lodCount);

	PtrSize seek = sizeof(m_header) + m_subMeshes.getSizeInBytes();

	for(U32 l = lod + 1; l < m_header.m_lodCount; ++l)
	{
		seek += getLodBuffersSize(l);
	}

	seek += getIndexBufferSize(lod);

	for(U32 i = 0; i < bufferIdx; ++i)
	{
		seek += getVertexBufferSize(lod, i);
	}

	ANKI_CHECK(m_file->seek(seek, FileSeekOrigin::BEGINNING));
	ANKI_CHECK(m_file->read(ptr, size));

	return Error::NONE;
}

Error MeshBinaryLoader::storeIndicesAndPosition(U32 lod, DynamicArrayAuto<U32>& indices,
												DynamicArrayAuto<Vec3>& positions)
{
	ANKI_ASSERT(isLoaded());
	ANKI_ASSERT(lod < m_header.m_lodCount);

	// Store indices
	{
		indices.resize(m_header.m_totalIndexCounts[lod]);

		// Store to staging buff
		DynamicArrayAuto<U8, PtrSize> staging(m_alloc);
		staging.create(getIndexBufferSize(lod));
		ANKI_CHECK(storeIndexBuffer(lod, &staging[0], staging.getSizeInBytes()));

		// Copy from staging
		ANKI_ASSERT(m_header.m_indexType == IndexType::U16);
		for(U32 i = 0; i < m_header.m_totalIndexCounts[lod]; ++i)
		{
			indices[i] = *reinterpret_cast<U16*>(&staging[PtrSize(i) * 2]);
		}
	}

	// Store positions
	{
		positions.resize(m_header.m_totalVertexCounts[lod]);
		const MeshBinaryVertexAttribute& attrib = m_header.m_vertexAttributes[VertexStreamId::POSITIONS];
		ANKI_CHECK(storeVertexBuffer(lod, attrib.m_bufferBinding, &positions[0], positions.getSizeInBytes()));
	}

	return Error::NONE;
}

PtrSize MeshBinaryLoader::getLodBuffersSize(U32 lod) const
{
	ANKI_ASSERT(lod < m_header.m_lodCount);

	PtrSize size = getIndexBufferSize(lod);
	for(U32 vertBufferIdx = 0; vertBufferIdx < m_header.m_vertexBufferCount; ++vertBufferIdx)
	{
		size += getVertexBufferSize(lod, vertBufferIdx);
	}

	return size;
}

} // end namespace anki
