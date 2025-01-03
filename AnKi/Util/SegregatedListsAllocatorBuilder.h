// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Util/Array.h>
#include <AnKi/Util/DynamicArray.h>
#include <AnKi/Util/StringList.h>

namespace anki {

/// @addtogroup util_memory
/// @{

namespace detail {

/// Free block.
/// @memberof SegregatedListsAllocatorBuilder
/// @internal
class SegregatedListsAllocatorBuilderFreeBlock
{
public:
	PtrSize m_size = 0;
	PtrSize m_address = 0;
};

} // end namespace detail

/// The base class for all user memory chunks of SegregatedListsAllocatorBuilder.
/// @memberof SegregatedListsAllocatorBuilder
template<typename TMemoryPool>
class SegregatedListsAllocatorBuilderChunkBase
{
	template<typename, typename, typename, typename>
	friend class SegregatedListsAllocatorBuilder;

protected:
	SegregatedListsAllocatorBuilderChunkBase(const TMemoryPool& pool = TMemoryPool())
		: m_freeLists(pool)
	{
	}

private:
	DynamicArray<DynamicArray<detail::SegregatedListsAllocatorBuilderFreeBlock, TMemoryPool>, TMemoryPool> m_freeLists;
	PtrSize m_totalSize = 0;
	PtrSize m_freeSize = 0;
};

/// It provides the tools to build allocators base on segregated lists and best fit.
/// @tparam TChunk A user defined class that contains a memory chunk. It should inherit from SegregatedListsAllocatorBuilderChunkBase.
/// @tparam TInterface The interface that contains the following members:
///                    @code
///                    /// The number of classes
///                    U32 getClassCount() const;
///                    /// Max size for each class.
///                    void getClassInfo(U32 idx, PtrSize& size) const;
///                    /// Allocates a new user defined chunk of memory.
///                    Error allocateChunk(TChunk*& newChunk, PtrSize& chunkSize);
///                    /// Deletes a chunk.
///                    void deleteChunk(TChunk* chunk);
///                    /// Get the min alignment that will be required.
///                    PtrSize getMinSizeAlignment() const;
///                    @endcode
/// @tparam TLock User defined lock (eg Mutex).
template<typename TChunk, typename TInterface, typename TLock, typename TMemoryPool>
class SegregatedListsAllocatorBuilder
{
public:
	SegregatedListsAllocatorBuilder(const TMemoryPool& pool = TMemoryPool())
		: m_chunks(pool)
	{
	}

	~SegregatedListsAllocatorBuilder();

	SegregatedListsAllocatorBuilder(const SegregatedListsAllocatorBuilder&) = delete;

	SegregatedListsAllocatorBuilder& operator=(const SegregatedListsAllocatorBuilder&) = delete;

	/// Allocate memory.
	/// @param size The size to allocate.
	/// @param alignment The alignment of the returned address. No need to be power of 2.
	/// @param[out] chunk The chunk that the memory belongs to.
	/// @param[out] offset The offset inside the chunk.
	/// @note This is thread safe.
	Error allocate(PtrSize size, PtrSize alignment, TChunk*& chunk, PtrSize& offset);

	/// Free memory.
	/// @param chunk The chunk the allocation belongs to.
	/// @param offset The memory offset inside the chunk.
	void free(TChunk* chunk, PtrSize offset, PtrSize size);

	/// Validate the internal structures. It's only used in testing.
	Error validate() const;

	/// Print debug info.
	void printFreeBlocks(StringList& strList) const;

	/// It's 1-(largestBlockOfFreeMemory/totalFreeMemory). 0.0 is no fragmentation, 1.0 is totally fragmented.
	[[nodiscard]] F32 computeExternalFragmentation(PtrSize baseSize = 1) const;

	/// Adam Sawicki metric. 0.0 is no fragmentation, 1.0 is totally fragmented.
	[[nodiscard]] F32 computeExternalFragmentationSawicki(PtrSize baseSize = 1) const;

	TLock& getLock() const
	{
		return m_lock;
	}

	TInterface& getInterface()
	{
		return m_interface;
	}

private:
	using FreeBlock = detail::SegregatedListsAllocatorBuilderFreeBlock;
	using ChunksIterator = typename DynamicArray<TChunk*>::Iterator;

	TInterface m_interface; ///< The interface.

	DynamicArray<TChunk*, TMemoryPool> m_chunks;

	mutable TLock m_lock;

	TMemoryPool& getMemoryPool()
	{
		return m_chunks.getMemoryPool();
	}

	U32 findClass(PtrSize size, PtrSize alignment) const;

	/// Choose the best free block out of 2 given the allocation size and alignment.
	/// @return True if the block returned is the best candidate overall.
	static Bool chooseBestFit(PtrSize allocSize, PtrSize allocAlignment, FreeBlock* blockA, FreeBlock* blockB, FreeBlock*& bestBlock);

	/// Place a free block in one of the lists.
	void placeFreeBlock(PtrSize address, PtrSize size, ChunksIterator chunkIt);
};
/// @}

} // end namespace anki

#include <AnKi/Util/SegregatedListsAllocatorBuilder.inl.h>
