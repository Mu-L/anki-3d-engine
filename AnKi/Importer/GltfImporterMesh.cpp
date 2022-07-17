// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Importer/GltfImporter.h>
#include <AnKi/Util/StringList.h>
#include <AnKi/Collision/Plane.h>
#include <AnKi/Collision/Functions.h>
#include <AnKi/Resource/MeshBinary.h>
#include <AnKi/Shaders/Include/MeshTypes.h>
#include <MeshOptimizer/meshoptimizer.h>

namespace anki {

static U cgltfComponentCount(cgltf_type type)
{
	U out;
	switch(type)
	{
	case cgltf_type_scalar:
		out = 1;
		break;
	case cgltf_type_vec2:
		out = 2;
		break;
	case cgltf_type_vec3:
		out = 3;
		break;
	case cgltf_type_vec4:
		out = 4;
		break;
	default:
		ANKI_ASSERT(!"TODO");
		out = 0;
	}

	return out;
}

static U cgltfComponentSize(cgltf_component_type type)
{
	U out;
	switch(type)
	{
	case cgltf_component_type_r_32f:
		out = sizeof(F32);
		break;
	case cgltf_component_type_r_16u:
		out = sizeof(U16);
		break;
	case cgltf_component_type_r_8u:
		out = sizeof(U8);
		break;
	default:
		ANKI_ASSERT(!"TODO");
		out = 0;
	}

	return out;
}

#if 0
static U calcImplicitStride(const cgltf_attribute& attrib)
{
	return cgltfComponentCount(attrib.data->type) * cgltfComponentSize(attrib.data->component_type);
}
#endif

template<typename T>
static Error checkAttribute(const cgltf_attribute& attrib)
{
	if(cgltfComponentCount(attrib.data->type) != T::COMPONENT_COUNT)
	{
		ANKI_IMPORTER_LOGE("Wrong component count for attribute: %s", attrib.name);
		return Error::USER_DATA;
	}

	if(cgltfComponentSize(attrib.data->component_type) != sizeof(typename T::Scalar))
	{
		ANKI_IMPORTER_LOGE("Incompatible type: %s", attrib.name);
		return Error::USER_DATA;
	}

	ANKI_ASSERT(attrib.data);
	const U count = U(attrib.data->count);
	if(count == 0)
	{
		ANKI_IMPORTER_LOGE("Zero vertex count");
		return Error::USER_DATA;
	}

	return Error::NONE;
}

class TempVertex
{
public:
	Vec3 m_position;
	F32 m_padding0;
	Vec4 m_tangent;
	Vec4 m_boneWeights;
	Vec3 m_normal;
	Vec2 m_uv;
	U16Vec4 m_boneIds;
	F32 m_padding1;

	TempVertex()
	{
		zeroMemory(*this);
	}
};
static_assert(sizeof(TempVertex) == 5 * sizeof(Vec4), "Will be hashed");

class SubMesh
{
public:
	DynamicArrayAuto<TempVertex> m_verts;
	DynamicArrayAuto<U32> m_indices;

	Vec3 m_aabbMin = Vec3(MAX_F32);
	Vec3 m_aabbMax = Vec3(MIN_F32);

	U32 m_firstIdx = MAX_U32;

	SubMesh(GenericMemoryPoolAllocator<U8>& alloc)
		: m_verts(alloc)
		, m_indices(alloc)
	{
	}
};

static void reindexSubmesh(SubMesh& submesh, GenericMemoryPoolAllocator<U8> alloc)
{
	const U32 vertSize = sizeof(submesh.m_verts[0]);

	DynamicArrayAuto<U32> remap(alloc);
	remap.create(submesh.m_verts.getSize(), 0);

	const U32 vertCount = U32(meshopt_generateVertexRemap(&remap[0], &submesh.m_indices[0], submesh.m_indices.getSize(),
														  &submesh.m_verts[0], submesh.m_verts.getSize(), vertSize));

	DynamicArrayAuto<U32> newIdxArray(alloc);
	newIdxArray.create(submesh.m_indices.getSize(), 0);
	DynamicArrayAuto<TempVertex> newVertArray(alloc);
	newVertArray.create(vertCount);

	meshopt_remapIndexBuffer(&newIdxArray[0], &submesh.m_indices[0], submesh.m_indices.getSize(), &remap[0]);
	meshopt_remapVertexBuffer(&newVertArray[0], &submesh.m_verts[0], submesh.m_verts.getSize(), vertSize, &remap[0]);

	submesh.m_indices = std::move(newIdxArray);
	submesh.m_verts = std::move(newVertArray);
}

/// Optimize a submesh using meshoptimizer.
static void optimizeSubmesh(SubMesh& submesh, GenericMemoryPoolAllocator<U8> alloc)
{
	const PtrSize vertSize = sizeof(submesh.m_verts[0]);

	// Vert cache
	{
		DynamicArrayAuto<U32> newIdxArray(alloc);
		newIdxArray.create(submesh.m_indices.getSize(), 0);

		meshopt_optimizeVertexCache(&newIdxArray[0], &submesh.m_indices[0], submesh.m_indices.getSize(),
									submesh.m_verts.getSize());

		submesh.m_indices = std::move(newIdxArray);
	}

	// Overdraw
	{
		DynamicArrayAuto<U32> newIdxArray(alloc);
		newIdxArray.create(submesh.m_indices.getSize(), 0);

		meshopt_optimizeOverdraw(&newIdxArray[0], &submesh.m_indices[0], submesh.m_indices.getSize(),
								 &submesh.m_verts[0].m_position.x(), submesh.m_verts.getSize(), vertSize, 1.05f);

		submesh.m_indices = std::move(newIdxArray);
	}

	// Vert fetch
	{
		DynamicArrayAuto<TempVertex> newVertArray(alloc);
		newVertArray.create(submesh.m_verts.getSize());

		const U32 newVertCount = U32(meshopt_optimizeVertexFetch(&newVertArray[0],
																 &submesh.m_indices[0], // Inplace
																 submesh.m_indices.getSize(), &submesh.m_verts[0],
																 submesh.m_verts.getSize(), vertSize));

		if(newVertCount != submesh.m_verts.getSize())
		{
			newVertArray.resize(newVertCount);
		}
		ANKI_ASSERT(newVertArray.getSize() == newVertCount);

		submesh.m_verts = std::move(newVertArray);
	}
}

/// Decimate a submesh using meshoptimizer.
static void decimateSubmesh(F32 factor, SubMesh& submesh, GenericMemoryPoolAllocator<U8> alloc)
{
	ANKI_ASSERT(factor > 0.0f && factor < 1.0f);
	const PtrSize targetIndexCount = PtrSize(F32(submesh.m_indices.getSize() / 3) * factor) * 3;
	if(targetIndexCount == 0)
	{
		return;
	}

	// Decimate
	DynamicArrayAuto<U32> newIndices(alloc, submesh.m_indices.getSize());
	newIndices.resize(U32(meshopt_simplify(&newIndices[0], &submesh.m_indices[0], submesh.m_indices.getSize(),
										   &submesh.m_verts[0].m_position.x(), submesh.m_verts.getSize(),
										   sizeof(TempVertex), targetIndexCount, 1e-2f)));

	// Re-pack
	DynamicArrayAuto<U32> reindexedIndices(alloc);
	DynamicArrayAuto<TempVertex> newVerts(alloc);
	HashMapAuto<U32, U32> vertexStored(alloc);
	for(U32 idx = 0; idx < newIndices.getSize(); ++idx)
	{
		U32 newIdx;
		auto it = vertexStored.find(newIndices[idx]);
		if(it == vertexStored.getEnd())
		{
			// Store the vertex
			newVerts.emplaceBack(submesh.m_verts[newIndices[idx]]);
			newIdx = newVerts.getSize() - 1;
			vertexStored.emplace(newIndices[idx], newIdx);
		}
		else
		{
			// Already stored
			newIdx = *it;
		}

		// Store the new index
		reindexedIndices.emplaceBack(newIdx);
	}

	// Move back
	submesh.m_indices = std::move(reindexedIndices);
	submesh.m_verts = std::move(newVerts);
}

static void computeSubmeshTangents(SubMesh& submesh)
{
	GenericMemoryPoolAllocator<U8> alloc = submesh.m_indices.getAllocator();
	const U32 vertCount = submesh.m_verts.getSize();
	ANKI_ASSERT(vertCount > 0);

	DynamicArrayAuto<Vec3> bitangents(alloc);
	bitangents.create(vertCount, Vec3(0.0f));

	for(U32 i = 0; i < submesh.m_indices.getSize(); i += 3)
	{
		const U32 i0 = submesh.m_indices[i + 0];
		const U32 i1 = submesh.m_indices[i + 1];
		const U32 i2 = submesh.m_indices[i + 2];

		const Vec3& v0 = submesh.m_verts[i0].m_position;
		const Vec3& v1 = submesh.m_verts[i1].m_position;
		const Vec3& v2 = submesh.m_verts[i2].m_position;
		const Vec3 edge01 = v1 - v0;
		const Vec3 edge02 = v2 - v0;

		const Vec2 uvedge01 = submesh.m_verts[i1].m_uv - submesh.m_verts[i0].m_uv;
		const Vec2 uvedge02 = submesh.m_verts[i2].m_uv - submesh.m_verts[i0].m_uv;

		F32 det = (uvedge01.y() * uvedge02.x()) - (uvedge01.x() * uvedge02.y());
		det = (isZero(det)) ? 0.0001f : (1.0f / det);

		Vec3 t = (edge02 * uvedge01.y() - edge01 * uvedge02.y()) * det;
		Vec3 b = (edge02 * uvedge01.x() - edge01 * uvedge02.x()) * det;

		if(t.getLengthSquared() < EPSILON)
		{
			t = Vec3(1.0f, 0.0f, 0.0f); // Something random
		}
		else
		{
			t.normalize();
		}

		if(b.getLengthSquared() < EPSILON)
		{
			b = Vec3(0.0f, 1.0f, 0.0f); // Something random
		}
		else
		{
			b.normalize();
		}

		submesh.m_verts[i0].m_tangent += Vec4(t, 0.0f);
		submesh.m_verts[i1].m_tangent += Vec4(t, 0.0f);
		submesh.m_verts[i2].m_tangent += Vec4(t, 0.0f);

		bitangents[i0] += b;
		bitangents[i1] += b;
		bitangents[i2] += b;
	}

	for(U32 i = 0; i < vertCount; ++i)
	{
		Vec3 t = Vec3(submesh.m_verts[i].m_tangent.xyz());
		const Vec3& n = submesh.m_verts[i].m_normal;
		Vec3& b = bitangents[i];

		if(t.getLengthSquared() < EPSILON)
		{
			t = Vec3(1.0f, 0.0f, 0.0f); // Something random
		}
		else
		{
			t.normalize();
		}

		if(b.getLengthSquared() < EPSILON)
		{
			b = Vec3(0.0f, 1.0f, 0.0f); // Something random
		}
		else
		{
			b.normalize();
		}

		const F32 w = ((n.cross(t)).dot(b) < 0.0f) ? 1.0f : -1.0f;
		submesh.m_verts[i].m_tangent = Vec4(t, w);
	}
}

/// Check if all submeshes form a convex shape.
static Bool submessesAreConvexShape(const ListAuto<SubMesh>& submeshes)
{
	Bool convex = true;
	for(const SubMesh& submesh : submeshes)
	{
		for(U32 i = 0; i < submesh.m_indices.getSize(); i += 3)
		{
			const U32 i0 = submesh.m_indices[i + 0];
			const U32 i1 = submesh.m_indices[i + 1];
			const U32 i2 = submesh.m_indices[i + 2];

			const Vec3& v0 = submesh.m_verts[i0].m_position;
			const Vec3& v1 = submesh.m_verts[i1].m_position;
			const Vec3& v2 = submesh.m_verts[i2].m_position;

			if(computeTriangleArea(v0, v1, v2) <= EPSILON)
			{
				continue;
			}

			// Check that all positions are behind the plane
			const Plane plane(v0.xyz0(), v1.xyz0(), v2.xyz0());

			for(const SubMesh& submeshB : submeshes)
			{
				for(const TempVertex& vertB : submeshB.m_verts)
				{
					const F32 test = testPlane(plane, vertB.m_position.xyz0());
					if(test > EPSILON)
					{
						convex = false;
						break;
					}
				}

				if(!convex)
				{
					break;
				}
			}

			if(!convex)
			{
				break;
			}
		}
	}

	return convex;
}

/// If normal A and normal B have the same position then try to merge them. Do that before optimizations.
static void fixSubMeshNormals(const F32 normalsMergeAngle, SubMesh& submesh)
{
	const U32 vertCount = submesh.m_verts.getSize();

	for(U32 v = 0; v < vertCount; ++v)
	{
		const Vec3& pos = submesh.m_verts[v].m_position;
		Vec3& normal = submesh.m_verts[v].m_normal;

		for(U32 prevV = 0; prevV < v; ++prevV)
		{
			const Vec3& otherPos = submesh.m_verts[prevV].m_position;

			// Check the positions dist
			const F32 posDist = (otherPos - pos).getLengthSquared();
			if(posDist > EPSILON * EPSILON)
			{
				continue;
			}

			// Check angle of the normals
			Vec3& otherNormal = submesh.m_verts[prevV].m_normal;
			const F32 ang = acos(clamp(otherNormal.dot(normal), -1.0f, 1.0f));
			if(ang > normalsMergeAngle)
			{
				continue;
			}

			// Merge normals
			const Vec3 newNormal = (otherNormal + normal).getNormalized();
			normal = newNormal;
			otherNormal = newNormal;
		}
	}
}

static void writeVertexAttribAndBufferInfoToHeader(VertexStreamId stream, MeshBinaryHeader& header)
{
	MeshBinaryVertexAttribute& attrib = header.m_vertexAttributes[stream];
	attrib.m_bufferBinding = U32(stream);
	attrib.m_format = REGULAR_VERTEX_STREAM_FORMATS[stream];
	attrib.m_relativeOffset = 0;
	attrib.m_scale = 1.0f;

	MeshBinaryVertexBuffer& buff = header.m_vertexBuffers[stream];
	buff.m_vertexStride = getFormatInfo(attrib.m_format).m_texelSize;
}

U32 GltfImporter::getMeshTotalVertexCount(const cgltf_mesh& mesh)
{
	U32 totalVertexCount = 0;

	for(const cgltf_primitive* primitive = mesh.primitives; primitive < mesh.primitives + mesh.primitives_count;
		++primitive)
	{
		totalVertexCount += U32(primitive->attributes[0].data->count);
	}

	return totalVertexCount;
}

Error GltfImporter::writeMesh(const cgltf_mesh& mesh)
{
	StringAuto fname(m_alloc);
	fname.sprintf("%s%s", m_outDir.cstr(), computeMeshResourceFilename(mesh).cstr());
	ANKI_IMPORTER_LOGV("Importing mesh (%s): %s", (m_optimizeMeshes) ? "optimize" : "WON'T optimize", fname.cstr());

	Array<ListAuto<SubMesh>, MAX_LOD_COUNT> submeshes = {{m_alloc, m_alloc, m_alloc}};
	Vec3 aabbMin(MAX_F32);
	Vec3 aabbMax(MIN_F32);
	Bool hasBoneWeights = false;

	// Iterate primitives. Every primitive is a submesh
	for(const cgltf_primitive* primitive = mesh.primitives; primitive < mesh.primitives + mesh.primitives_count;
		++primitive)
	{
		if(primitive->type != cgltf_primitive_type_triangles)
		{
			ANKI_IMPORTER_LOGE("Expecting triangles got %d", primitive->type);
			return Error::USER_DATA;
		}

		SubMesh& submesh = *submeshes[0].emplaceBack(m_alloc);

		U minVertCount = MAX_U;
		U maxVertCount = MIN_U;
		for(const cgltf_attribute* attrib = primitive->attributes;
			attrib < primitive->attributes + primitive->attributes_count; ++attrib)
		{
			minVertCount = min(minVertCount, U(attrib->data->count));
			maxVertCount = max(maxVertCount, U(attrib->data->count));
		}

		if(maxVertCount == 0 || minVertCount != maxVertCount)
		{
			ANKI_IMPORTER_LOGE("Wrong number of vertices");
			return Error::USER_DATA;
		}

		U32 vertCount = U32(primitive->attributes[0].data->count);
		submesh.m_verts.create(vertCount);

		//
		// Gather positions + normals + UVs
		//
		for(const cgltf_attribute* attrib = primitive->attributes;
			attrib < primitive->attributes + primitive->attributes_count; ++attrib)
		{
			if(attrib->type == cgltf_attribute_type_position)
			{
				U32 count = 0;
				ANKI_CHECK(checkAttribute<Vec3>(*attrib));
				visitAccessor<Vec3>(*attrib->data, [&](const Vec3& pos) {
					submesh.m_aabbMin = submesh.m_aabbMin.min(pos);
					submesh.m_aabbMax = submesh.m_aabbMax.max(pos);
					submesh.m_verts[count++].m_position = pos;
				});
			}
			else if(attrib->type == cgltf_attribute_type_normal)
			{
				U32 count = 0;
				ANKI_CHECK(checkAttribute<Vec3>(*attrib));
				visitAccessor<Vec3>(*attrib->data, [&](const Vec3& normal) {
					submesh.m_verts[count++].m_normal = normal;
				});
			}
			else if(attrib->type == cgltf_attribute_type_texcoord && CString(attrib->name) == "TEXCOORD_0")
			{
				U32 count = 0;
				ANKI_CHECK(checkAttribute<Vec2>(*attrib));
				visitAccessor<Vec2>(*attrib->data, [&](const Vec2& uv) {
					submesh.m_verts[count++].m_uv = uv;
				});
			}
			else if(attrib->type == cgltf_attribute_type_joints)
			{
				U32 count = 0;
				if(cgltfComponentSize(attrib->data->component_type) == 2)
				{
					ANKI_CHECK(checkAttribute<U16Vec4>(*attrib));
					visitAccessor<U16Vec4>(*attrib->data, [&](const U16Vec4& x) {
						submesh.m_verts[count++].m_boneIds = x;
					});
				}
				else
				{
					ANKI_CHECK(checkAttribute<U8Vec4>(*attrib));
					visitAccessor<U8Vec4>(*attrib->data, [&](const U8Vec4& x) {
						submesh.m_verts[count++].m_boneIds = U16Vec4(x);
					});
				}
				hasBoneWeights = true;
			}
			else if(attrib->type == cgltf_attribute_type_weights)
			{
				U32 count = 0;
				ANKI_CHECK(checkAttribute<Vec4>(*attrib));
				visitAccessor<Vec4>(*attrib->data, [&](const Vec4& bw) {
					submesh.m_verts[count++].m_boneWeights = bw;
				});
			}
			else
			{
				ANKI_IMPORTER_LOGV("Ignoring attribute: %s", attrib->name);
			}
		}

		aabbMin = aabbMin.min(submesh.m_aabbMin);
		// Bump aabbMax a bit
		submesh.m_aabbMax += EPSILON * 10.0f;
		aabbMax = aabbMax.max(submesh.m_aabbMax);

		// Fix normals
		fixSubMeshNormals(m_normalsMergeAngle, submesh);

		//
		// Load indices
		//
		{
			ANKI_ASSERT(primitive->indices);
			if(primitive->indices->count == 0 || (primitive->indices->count % 3) != 0)
			{
				ANKI_IMPORTER_LOGE("Incorect index count: %lu", primitive->indices->count);
				return Error::USER_DATA;
			}
			submesh.m_indices.create(U32(primitive->indices->count));
			const U8* base = static_cast<const U8*>(primitive->indices->buffer_view->buffer->data)
							 + primitive->indices->offset + primitive->indices->buffer_view->offset;
			for(U32 i = 0; i < primitive->indices->count; ++i)
			{
				U32 idx;
				if(primitive->indices->component_type == cgltf_component_type_r_32u)
				{
					idx = *reinterpret_cast<const U32*>(base + sizeof(U32) * i);
				}
				else if(primitive->indices->component_type == cgltf_component_type_r_16u)
				{
					idx = *reinterpret_cast<const U16*>(base + sizeof(U16) * i);
				}
				else
				{
					ANKI_ASSERT(0);
					idx = 0;
				}

				submesh.m_indices[i] = idx;
			}
		}

		// Re-index meshes now and
		// - before the tanget calculation because that will create many unique verts
		// - after normal fix because that will create verts with same attributes
		if(m_optimizeMeshes)
		{
			reindexSubmesh(submesh, m_alloc);
			vertCount = submesh.m_verts.getSize();
		}

		// Compute tangent
		computeSubmeshTangents(submesh);

		// Optimize
		if(m_optimizeMeshes)
		{
			optimizeSubmesh(submesh, m_alloc);
		}

		// Finalize
		if(submesh.m_indices.getSize() == 0 || submesh.m_verts.getSize() == 0)
		{
			// Digenerate
			submeshes[0].popBack();
		}
	}

	if(submeshes[0].getSize() == 0)
	{
		ANKI_IMPORTER_LOGE("Mesh contains degenerate geometry");
		return Error::USER_DATA;
	}

	// Generate submeshes for the other LODs
	ANKI_ASSERT(m_lodCount <= MAX_LOD_COUNT && m_lodCount > 0);
	U32 maxLod = 0;
	for(U32 lod = 1; lod < m_lodCount; ++lod)
	{
		if(skipMeshLod(mesh, lod))
		{
			break;
		}

		for(const SubMesh& lod0Submesh : submeshes[0])
		{
			SubMesh& newSubmesh = *submeshes[lod].pushBack(m_alloc);
			newSubmesh = lod0Submesh; // Copy LOD0 data to new submesh

			decimateSubmesh(computeLodFactor(lod), newSubmesh, m_alloc);
		}

		maxLod = lod;
	}

	// Write the header
	MeshBinaryHeader header;
	memset(&header, 0, sizeof(header));
	memcpy(&header.m_magic[0], MESH_MAGIC, 8);

	header.m_flags = MeshBinaryFlag::NONE;
	if(submessesAreConvexShape(submeshes[0]))
	{
		header.m_flags |= MeshBinaryFlag::CONVEX;
	}
	header.m_indexType = IndexType::U16;
	header.m_subMeshCount = U32(submeshes.getSize());
	header.m_aabbMin = aabbMin;
	header.m_aabbMax = aabbMax;

	writeVertexAttribAndBufferInfoToHeader(VertexStreamId::POSITION, header);
	writeVertexAttribAndBufferInfoToHeader(VertexStreamId::NORMAL, header);
	writeVertexAttribAndBufferInfoToHeader(VertexStreamId::TANGENT, header);
	writeVertexAttribAndBufferInfoToHeader(VertexStreamId::UV, header);
	if(hasBoneWeights)
	{
		writeVertexAttribAndBufferInfoToHeader(VertexStreamId::BONE_IDS, header);
		writeVertexAttribAndBufferInfoToHeader(VertexStreamId::BONE_WEIGHTS, header);
	}

	// Write sub meshes
	DynamicArrayAuto<MeshBinarySubMesh> outSubmeshes(m_alloc, U32(submeshes[0].getSize()));

	for(U32 submeshIdx = 0; submeshIdx < outSubmeshes.getSize(); ++submeshIdx)
	{
		MeshBinarySubMesh& out = outSubmeshes[submeshIdx];
		memset(&out, 0, sizeof(out));

		for(U32 lod = 0; lod <= maxLod; ++lod)
		{
			const SubMesh& inSubmesh = *(submeshes[lod].getBegin() + submeshIdx);

			if(lod == 0)
			{
				out.m_aabbMin = inSubmesh.m_aabbMin;
				out.m_aabbMax = inSubmesh.m_aabbMax;
			}

			out.m_firstIndices[lod] = header.m_totalIndexCounts[lod];
			out.m_indexCounts[lod] = inSubmesh.m_indices.getSize();

			out.m_firstVertices[lod] = header.m_totalVertexCounts[lod];
			out.m_vertexCounts[lod] = inSubmesh.m_verts.getSize();

			header.m_totalIndexCounts[lod] += inSubmesh.m_indices.getSize();
			header.m_totalVertexCounts[lod] += inSubmesh.m_verts.getSize();
		}
	}

	// Start writing the file
	File file;
	ANKI_CHECK(file.open(fname.toCString(), FileOpenFlag::WRITE | FileOpenFlag::BINARY));
	ANKI_CHECK(file.write(&header, sizeof(header)));
	ANKI_CHECK(file.write(&outSubmeshes[0], outSubmeshes.getSizeInBytes()));

	// Write LODs
	for(I32 lod = I32(maxLod); lod >= 0; --lod)
	{
		// Write index buffer
		U32 vertCount = 0;
		for(const SubMesh& submesh : submeshes[lod])
		{
			DynamicArrayAuto<U16> indices(m_alloc, submesh.m_indices.getSize());
			for(U32 i = 0; i < indices.getSize(); ++i)
			{
				const U32 idx = submesh.m_indices[i] + vertCount;
				if(idx > MAX_U16)
				{
					ANKI_IMPORTER_LOGE("Only supports 16bit indices for now (%u): %s", idx, fname.cstr());
					return Error::USER_DATA;
				}

				indices[i] = U16(idx);
			}

			ANKI_CHECK(file.write(&indices[0], indices.getSizeInBytes()));
			vertCount += submesh.m_verts.getSize();
		}

		// Write positions
		for(const SubMesh& submesh : submeshes[lod])
		{
			DynamicArrayAuto<Vec3> positions(m_alloc, submesh.m_verts.getSize());
			for(U32 v = 0; v < submesh.m_verts.getSize(); ++v)
			{
				positions[v] = submesh.m_verts[v].m_position;
			}

			ANKI_CHECK(file.write(&positions[0], positions.getSizeInBytes()));
		}

		// Write normals
		for(const SubMesh& submesh : submeshes[lod])
		{
			DynamicArrayAuto<U32> normals(m_alloc, submesh.m_verts.getSize());
			for(U32 v = 0; v < submesh.m_verts.getSize(); ++v)
			{
				normals[v] = packSnorm4x8(submesh.m_verts[v].m_normal.xyz0());
			}

			ANKI_CHECK(file.write(&normals[0], normals.getSizeInBytes()));
		}

		// Write tangent
		for(const SubMesh& submesh : submeshes[lod])
		{
			DynamicArrayAuto<U32> tangents(m_alloc, submesh.m_verts.getSize());
			for(U32 v = 0; v < submesh.m_verts.getSize(); ++v)
			{
				tangents[v] = packSnorm4x8(submesh.m_verts[v].m_tangent);
			}

			ANKI_CHECK(file.write(&tangents[0], tangents.getSizeInBytes()));
		}

		// Write UV
		for(const SubMesh& submesh : submeshes[lod])
		{
			DynamicArrayAuto<Vec2> uvs(m_alloc, submesh.m_verts.getSize());
			for(U32 v = 0; v < submesh.m_verts.getSize(); ++v)
			{
				uvs[v] = submesh.m_verts[v].m_uv;
			}

			ANKI_CHECK(file.write(&uvs[0], uvs.getSizeInBytes()));
		}

		if(hasBoneWeights)
		{
			// Bone IDs
			for(const SubMesh& submesh : submeshes[lod])
			{
				DynamicArrayAuto<UVec4> boneids(m_alloc, submesh.m_verts.getSize());
				for(U32 v = 0; v < submesh.m_verts.getSize(); ++v)
				{
					boneids[v] = UVec4(submesh.m_verts[v].m_boneIds);
				}

				ANKI_CHECK(file.write(&boneids[0], boneids.getSizeInBytes()));
			}

			// Bone weights
			for(const SubMesh& submesh : submeshes[lod])
			{
				DynamicArrayAuto<U32> boneWeights(m_alloc, submesh.m_verts.getSize());
				for(U32 v = 0; v < submesh.m_verts.getSize(); ++v)
				{
					boneWeights[v] = packSnorm4x8(submesh.m_verts[v].m_boneWeights);
				}

				ANKI_CHECK(file.write(&boneWeights[0], boneWeights.getSizeInBytes()));
			}
		}
	}

	return Error::NONE;
}

} // end namespace anki
