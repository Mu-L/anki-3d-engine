// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Core/Common.h>
#include <AnKi/Gr/Utils/StackGpuMemoryPool.h>
#include <AnKi/Gr/GrManager.h>
#include <AnKi/Gr/Buffer.h>

namespace anki {

/// @addtogroup core
/// @{

/// @memberof GpuVisibleTransientMemoryPool
class GpuVisibleTransientMemoryAllocation
{
	friend class GpuVisibleTransientMemoryPool;

public:
	Buffer& getBuffer() const
	{
		ANKI_ASSERT(isValid());
		return *m_buffer;
	}

	PtrSize getOffset() const
	{
		ANKI_ASSERT(isValid());
		return m_offset;
	}

	PtrSize getRange() const
	{
		ANKI_ASSERT(isValid());
		return m_size;
	}

	Bool isValid() const
	{
		return m_buffer != nullptr;
	}

	operator BufferView() const;

private:
	Buffer* m_buffer = nullptr;
	PtrSize m_offset = kMaxPtrSize;
	PtrSize m_size = 0;
};

/// GPU only transient memory. Used for temporary allocations. Allocations will get reset after each frame.
class GpuVisibleTransientMemoryPool : public MakeSingleton<GpuVisibleTransientMemoryPool>
{
	template<typename>
	friend class MakeSingleton;

public:
	GpuVisibleTransientMemoryAllocation allocate(PtrSize size)
	{
		GpuVisibleTransientMemoryAllocation out;
		m_pool.allocate(size, out.m_offset, out.m_buffer);
		out.m_size = size;
		return out;
	}

	void endFrame();

private:
	StackGpuMemoryPool m_pool;
	U32 m_frame = 0;

	GpuVisibleTransientMemoryPool()
	{
		U32 alignment = GrManager::getSingleton().getDeviceCapabilities().m_uniformBufferBindOffsetAlignment;
		alignment = max(alignment, GrManager::getSingleton().getDeviceCapabilities().m_storageBufferBindOffsetAlignment);
		alignment = max(alignment, GrManager::getSingleton().getDeviceCapabilities().m_sbtRecordAlignment);
		alignment = max(alignment, GrManager::getSingleton().getDeviceCapabilities().m_accelerationStructureBuildScratchOffsetAlignment);

		BufferUsageBit buffUsage = BufferUsageBit::kAllUniform | BufferUsageBit::kAllStorage | BufferUsageBit::kIndirectDraw
								   | BufferUsageBit::kIndirectCompute | BufferUsageBit::kVertex | BufferUsageBit::kTransferDestination;
		if(GrManager::getSingleton().getDeviceCapabilities().m_rayTracingEnabled)
		{
			buffUsage |= (BufferUsageBit::kAccelerationStructureBuildScratch | BufferUsageBit::kAccelerationStructureBuild);
		}
		m_pool.init(10_MB, 2.0, 0, alignment, buffUsage, BufferMapAccessBit::kNone, true, "GpuVisibleTransientMemoryPool");
	}

	~GpuVisibleTransientMemoryPool() = default;
};

inline GpuVisibleTransientMemoryAllocation::operator BufferView() const
{
	ANKI_ASSERT(isValid());
	return {m_buffer, m_offset, m_size};
}
/// @}

} // end namespace anki