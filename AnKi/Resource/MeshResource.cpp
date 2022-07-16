// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Resource/MeshResource.h>
#include <AnKi/Resource/ResourceManager.h>
#include <AnKi/Resource/MeshBinaryLoader.h>
#include <AnKi/Resource/AsyncLoader.h>
#include <AnKi/Core/GpuMemoryPools.h>
#include <AnKi/Util/Functions.h>
#include <AnKi/Util/Filesystem.h>

namespace anki {

class MeshResource::LoadContext
{
public:
	MeshResourcePtr m_mesh;
	MeshBinaryLoader m_loader;

	LoadContext(const MeshResourcePtr& mesh, GenericMemoryPoolAllocator<U8> alloc)
		: m_mesh(mesh)
		, m_loader(&mesh->getManager(), alloc)
	{
	}
};

/// Mesh upload async task.
class MeshResource::LoadTask : public AsyncLoaderTask
{
public:
	MeshResource::LoadContext m_ctx;

	LoadTask(const MeshResourcePtr& mesh)
		: m_ctx(mesh, mesh->getManager().getAsyncLoader().getAllocator())
	{
	}

	Error operator()([[maybe_unused]] AsyncLoaderTaskContext& ctx) final
	{
		return m_ctx.m_mesh->loadAsync(m_ctx.m_loader);
	}

	GenericMemoryPoolAllocator<U8> getAllocator() const
	{
		return m_ctx.m_mesh->getManager().getAsyncLoader().getAllocator();
	}
};

MeshResource::MeshResource(ResourceManager* manager)
	: ResourceObject(manager)
{
}

MeshResource::~MeshResource()
{
	m_subMeshes.destroy(getAllocator());

	for(Lod& lod : m_lods)
	{
		if(lod.m_globalIndexBufferOffset != MAX_PTR_SIZE)
		{
			const U32 alignment = getIndexSize(m_indexType);
			const PtrSize size = lod.m_indexCount * PtrSize(alignment);
			getManager().getVertexGpuMemory().free(size, alignment, lod.m_globalIndexBufferOffset);
		}

		for(VertexStreamId stream : EnumIterable<VertexStreamId>())
		{
			if(lod.m_globalVertexBufferOffsets[stream] != MAX_PTR_SIZE)
			{
				const U32 alignment = getFormatInfo(REGULAR_VERTEX_STREAM_FORMATS[stream]).m_texelSize;
				const PtrSize size = PtrSize(alignment) * lod.m_vertexCount;

				getManager().getVertexGpuMemory().free(size, alignment, lod.m_globalVertexBufferOffsets[stream]);
			}
		}
	}

	m_lods.destroy(getAllocator());
}

Error MeshResource::load(const ResourceFilename& filename, Bool async)
{
	UniquePtr<LoadTask> task;
	LoadContext* ctx;
	LoadContext localCtx(MeshResourcePtr(this), getTempAllocator());

	StringAuto basename(getTempAllocator());
	getFilepathFilename(filename, basename);

	const Bool rayTracingEnabled = getManager().getGrManager().getDeviceCapabilities().m_rayTracingEnabled;
	BufferPtr globalVertexIndexBuffer = getManager().getVertexGpuMemory().getVertexBuffer();

	if(async)
	{
		task.reset(getManager().getAsyncLoader().newTask<LoadTask>(MeshResourcePtr(this)));
		ctx = &task->m_ctx;
	}
	else
	{
		task.reset(nullptr);
		ctx = &localCtx;
	}

	// Open file
	MeshBinaryLoader& loader = ctx->m_loader;
	ANKI_CHECK(loader.load(filename));
	const MeshBinaryHeader& header = loader.getHeader();

	// Misc
	m_indexType = header.m_indexType;
	m_aabb.setMin(header.m_aabbMin);
	m_aabb.setMax(header.m_aabbMax);

	// Submeshes
	m_subMeshes.create(getAllocator(), header.m_subMeshCount);
	for(U32 i = 0; i < m_subMeshes.getSize(); ++i)
	{
		m_subMeshes[i].m_firstIndices = loader.getSubMeshes()[i].m_firstIndices;
		m_subMeshes[i].m_indexCounts = loader.getSubMeshes()[i].m_indexCounts;
		m_subMeshes[i].m_aabb.setMin(loader.getSubMeshes()[i].m_aabbMin);
		m_subMeshes[i].m_aabb.setMax(loader.getSubMeshes()[i].m_aabbMax);
	}

	// LODs
	m_lods.create(getAllocator(), header.m_lodCount);
	for(U32 l = header.m_lodCount - 1; l >= 0; --l)
	{
		Lod& lod = m_lods[l];

		// Index stuff
		lod.m_indexCount = header.m_totalIndexCounts[l];
		ANKI_ASSERT((lod.m_indexCount % 3) == 0 && "Expecting triangles");
		const PtrSize indexBufferSize = PtrSize(lod.m_indexCount) * getIndexSize(m_indexType);
		ANKI_CHECK(getManager().getVertexGpuMemory().allocate(indexBufferSize, getIndexSize(m_indexType),
															  lod.m_globalIndexBufferOffset));

		// Vertex stuff
		lod.m_vertexCount = header.m_totalVertexCounts[l];
		for(VertexStreamId stream : EnumIterable<VertexStreamId>())
		{
			if(header.m_vertexAttributes[stream].m_format == Format::NONE)
			{
				lod.m_globalVertexBufferOffsets[stream] = MAX_PTR_SIZE;
				continue;
			}

			m_presentVertStreams |= VertexStreamBit(1 << stream);

			const U32 texelSize = getFormatInfo(REGULAR_VERTEX_STREAM_FORMATS[stream]).m_texelSize;
			const PtrSize vertexBufferSize = PtrSize(lod.m_vertexCount) * texelSize;

			ANKI_CHECK(getManager().getVertexGpuMemory().allocate(vertexBufferSize, texelSize,
																  lod.m_globalVertexBufferOffsets[stream]));
		}

		// BLAS
		if(rayTracingEnabled)
		{
			AccelerationStructureInitInfo inf(StringAuto(getTempAllocator()).sprintf("%s_%s", "Blas", basename.cstr()));
			inf.m_type = AccelerationStructureType::BOTTOM_LEVEL;

			inf.m_bottomLevel.m_indexBuffer = globalVertexIndexBuffer;
			inf.m_bottomLevel.m_indexBufferOffset = lod.m_globalIndexBufferOffset;
			inf.m_bottomLevel.m_indexCount = lod.m_indexCount;
			inf.m_bottomLevel.m_indexType = m_indexType;
			inf.m_bottomLevel.m_positionBuffer = globalVertexIndexBuffer;
			inf.m_bottomLevel.m_positionBufferOffset = lod.m_globalVertexBufferOffsets[VertexStreamId::POSITION];
			inf.m_bottomLevel.m_positionStride =
				getFormatInfo(REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::POSITION]).m_texelSize;
			inf.m_bottomLevel.m_positionsFormat = REGULAR_VERTEX_STREAM_FORMATS[VertexStreamId::POSITION];
			inf.m_bottomLevel.m_positionCount = lod.m_vertexCount;

			lod.m_blas = getManager().getGrManager().newAccelerationStructure(inf);
		}
	}

	// Clear the buffers
	if(async)
	{
		CommandBufferInitInfo cmdbinit("MeshResourceClear");
		cmdbinit.m_flags = CommandBufferFlag::SMALL_BATCH | CommandBufferFlag::GENERAL_WORK;
		CommandBufferPtr cmdb = getManager().getGrManager().newCommandBuffer(cmdbinit);

		for(const Lod& lod : m_lods)
		{
			cmdb->fillBuffer(globalVertexIndexBuffer, lod.m_globalIndexBufferOffset,
							 PtrSize(lod.m_indexCount) * getIndexSize(m_indexType), 0);

			for(VertexStreamId stream : EnumIterable<VertexStreamId>())
			{
				if(header.m_vertexAttributes[stream].m_format != Format::NONE)
				{
					cmdb->fillBuffer(globalVertexIndexBuffer, lod.m_globalVertexBufferOffsets[stream],
									 PtrSize(lod.m_vertexCount)
										 * getFormatInfo(REGULAR_VERTEX_STREAM_FORMATS[stream]).m_texelSize,
									 0);
				}
			}
		}

		cmdb->setBufferBarrier(globalVertexIndexBuffer, BufferUsageBit::TRANSFER_DESTINATION, BufferUsageBit::ALL_READ,
							   0, MAX_PTR_SIZE);
		cmdb->flush();
	}

	// Submit the loading task
	if(async)
	{
		getManager().getAsyncLoader().submitTask(task.get());
		LoadTask* pTask;
		task.moveAndReset(pTask);
	}
	else
	{
		ANKI_CHECK(loadAsync(loader));
	}

	return Error::NONE;
}

Error MeshResource::loadAsync(MeshBinaryLoader& loader) const
{
	GrManager& gr = getManager().getGrManager();
	TransferGpuAllocator& transferAlloc = getManager().getTransferGpuAllocator();

	Array<TransferGpuAllocatorHandle, MAX_LOD_COUNT*(U32(VertexStreamId::COUNT) + 1)> handles;
	U32 handleCount = 0;

	BufferPtr globalVertexIndexBuffer = getManager().getVertexGpuMemory().getVertexBuffer();

	CommandBufferInitInfo cmdbinit;
	cmdbinit.m_flags = CommandBufferFlag::SMALL_BATCH | CommandBufferFlag::GENERAL_WORK;
	CommandBufferPtr cmdb = gr.newCommandBuffer(cmdbinit);

	// Set transfer to transfer barrier because of the clear that happened while sync loading
	cmdb->setBufferBarrier(globalVertexIndexBuffer, BufferUsageBit::TRANSFER_DESTINATION,
						   BufferUsageBit::TRANSFER_DESTINATION, 0, MAX_PTR_SIZE);

	// Upload index and vertex buffers
	for(U32 lodIdx = 0; lodIdx < m_lods.getSize(); ++lodIdx)
	{
		const Lod& lod = m_lods[lodIdx];

		// Upload index buffer
		{
			TransferGpuAllocatorHandle& handle = handles[handleCount++];
			const PtrSize indexBufferSize = PtrSize(lod.m_indexCount) * getIndexSize(m_indexType);

			ANKI_CHECK(transferAlloc.allocate(indexBufferSize, handle));
			void* data = handle.getMappedMemory();
			ANKI_ASSERT(data);

			ANKI_CHECK(loader.storeIndexBuffer(lodIdx, data, indexBufferSize));

			cmdb->copyBufferToBuffer(handle.getBuffer(), handle.getOffset(), globalVertexIndexBuffer,
									 lod.m_globalIndexBufferOffset, handle.getRange());
		}

		// Upload vert buffers
		for(VertexStreamId stream : EnumIterable<VertexStreamId>())
		{
			if(!(m_presentVertStreams & VertexStreamBit(1 << stream)))
			{
				continue;
			}

			TransferGpuAllocatorHandle& handle = handles[handleCount++];
			const PtrSize vertexBufferSize =
				PtrSize(lod.m_vertexCount) * getFormatInfo(REGULAR_VERTEX_STREAM_FORMATS[stream]).m_texelSize;

			ANKI_CHECK(transferAlloc.allocate(vertexBufferSize, handle));
			U8* data = static_cast<U8*>(handle.getMappedMemory());
			ANKI_ASSERT(data);

			// Load to staging
			ANKI_CHECK(loader.storeVertexBuffer(lodIdx, U32(stream), data, vertexBufferSize));

			// Copy
			cmdb->copyBufferToBuffer(handle.getBuffer(), handle.getOffset(), globalVertexIndexBuffer,
									 lod.m_globalVertexBufferOffsets[stream], handle.getRange());
		}
	}

	if(gr.getDeviceCapabilities().m_rayTracingEnabled)
	{
		// Build BLASes

		// Set the barriers
		cmdb->setBufferBarrier(globalVertexIndexBuffer, BufferUsageBit::TRANSFER_DESTINATION,
							   BufferUsageBit::ACCELERATION_STRUCTURE_BUILD | BufferUsageBit::ALL_READ, 0,
							   MAX_PTR_SIZE);

		for(U32 lodIdx = 0; lodIdx < m_lods.getSize(); ++lodIdx)
		{
			cmdb->setAccelerationStructureBarrier(m_lods[lodIdx].m_blas, AccelerationStructureUsageBit::NONE,
												  AccelerationStructureUsageBit::BUILD);
		}

		// Build BLASes
		for(U32 lodIdx = 0; lodIdx < m_lods.getSize(); ++lodIdx)
		{
			cmdb->buildAccelerationStructure(m_lods[lodIdx].m_blas);
		}

		// Barriers again
		for(U32 lodIdx = 0; lodIdx < m_lods.getSize(); ++lodIdx)
		{
			cmdb->setAccelerationStructureBarrier(m_lods[lodIdx].m_blas, AccelerationStructureUsageBit::BUILD,
												  AccelerationStructureUsageBit::ALL_READ);
		}
	}
	else
	{
		// Only set a barrier
		cmdb->setBufferBarrier(globalVertexIndexBuffer, BufferUsageBit::TRANSFER_DESTINATION, BufferUsageBit::ALL_READ,
							   0, MAX_PTR_SIZE);
	}

	// Finalize
	FencePtr fence;
	cmdb->flush({}, &fence);

	for(U32 i = 0; i < handleCount; ++i)
	{
		transferAlloc.release(handles[i], fence);
	}

	return Error::NONE;
}

} // end namespace anki
