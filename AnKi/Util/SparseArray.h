// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Util/StdTypes.h>
#include <AnKi/Util/Assert.h>
#include <AnKi/Util/Array.h>
#include <utility>

namespace anki {

/// @addtogroup util_containers
/// @{

/// Sparse array iterator.
template<typename TValuePointer, typename TValueReference, typename TSparseArrayPtr>
class SparseArrayIterator
{
	template<typename, typename, typename>
	friend class SparseArray;

public:
	using Index = typename RemovePointer<TSparseArrayPtr>::Type::Index;

	/// Default constructor.
	SparseArrayIterator()
		: m_array(nullptr)
		, m_elementIdx(getMaxNumericLimit<Index>())
#if ANKI_EXTRA_CHECKS
		, m_iteratorVer(kMaxU32)
#endif
	{
	}

	/// Copy.
	SparseArrayIterator(const SparseArrayIterator& b)
		: m_array(b.m_array)
		, m_elementIdx(b.m_elementIdx)
#if ANKI_EXTRA_CHECKS
		, m_iteratorVer(b.m_iteratorVer)
#endif
	{
	}

	/// Allow conversion from iterator to const iterator.
	template<typename YValuePointer, typename YValueReference, typename YSparseArrayPtr>
	SparseArrayIterator(const SparseArrayIterator<YValuePointer, YValueReference, YSparseArrayPtr>& b)
		: m_array(b.m_array)
		, m_elementIdx(b.m_elementIdx)
#if ANKI_EXTRA_CHECKS
		, m_iteratorVer(b.m_iteratorVer)
#endif
	{
	}

	SparseArrayIterator(TSparseArrayPtr arr, Index modIdx
#if ANKI_EXTRA_CHECKS
						,
						U32 ver
#endif
						)
		: m_array(arr)
		, m_elementIdx(modIdx)
#if ANKI_EXTRA_CHECKS
		, m_iteratorVer(ver)
#endif
	{
		ANKI_ASSERT(arr);
	}

	SparseArrayIterator& operator=(const SparseArrayIterator& b)
	{
		m_array = b.m_array;
		m_elementIdx = b.m_elementIdx;
#if ANKI_EXTRA_CHECKS
		m_iteratorVer = b.m_iteratorVer;
#endif
		return *this;
	}

	TValueReference operator*() const
	{
		check();
		return m_array->m_elements[m_elementIdx];
	}

	TValuePointer operator->() const
	{
		check();
		return &m_array->m_elements[m_elementIdx];
	}

	SparseArrayIterator& operator++()
	{
		check();
		m_elementIdx = m_array->iterate(m_elementIdx, 1);
		return *this;
	}

	SparseArrayIterator operator++(int)
	{
		check();
		SparseArrayIterator out = *this;
		++(*this);
		return out;
	}

	SparseArrayIterator operator+(Index n) const
	{
		check();
		Index pos = m_array->iterate(m_elementIdx, n);
		return SparseArrayIterator(m_array, pos);
	}

	SparseArrayIterator& operator+=(Index n)
	{
		check();
		m_elementIdx = m_array->iterate(m_elementIdx, n);
		return *this;
	}

	Bool operator==(const SparseArrayIterator& b) const
	{
		ANKI_ASSERT(m_array == b.m_array);
		ANKI_ASSERT(m_iteratorVer == b.m_iteratorVer);
		return m_elementIdx == b.m_elementIdx;
	}

	Bool operator!=(const SparseArrayIterator& b) const
	{
		return !(*this == b);
	}

	Index getKey() const
	{
		check();
		return m_elementIdx;
	}

private:
	TSparseArrayPtr m_array;
	Index m_elementIdx;
#if ANKI_EXTRA_CHECKS
	U32 m_iteratorVer; ///< See SparseArray::m_iteratorVer.
#endif

	void check() const
	{
		ANKI_ASSERT(m_array);
		ANKI_ASSERT(m_elementIdx != getMaxNumericLimit<Index>());
		ANKI_ASSERT(m_array->m_metadata[m_elementIdx].m_alive);
		ANKI_ASSERT(m_array->m_iteratorVer == m_iteratorVer);
	}
};

/// Contains the default configuration for SparseArray.
/// @memberof SparseArray
class SparseArrayDefaultConfig
{
public:
	/// Indicates the max size of the sparse indices it can accept. Can be U32 or U64.
	using Index = U32;

	/// The initial storage size of the array.
	static constexpr Index getInitialStorageSize()
	{
		return 64;
	}

	/// The number of linear probes.
	static constexpr U32 getLinearProbingCount()
	{
		return 8;
	}

	/// Load factor. If storage is loaded more than getMaxLoadFactor() then increase it.
	static constexpr F32 getMaxLoadFactor()
	{
		return 0.8f;
	}
};

/// Sparse array.
/// @tparam T The type of the valut it will hold.
/// @tparam TConfig A class that has configuration required by the SparseArray. See SparseArrayDefaultConfig for
/// details.
template<typename T, typename TMemoryPool = SingletonMemoryPoolWrapper<DefaultMemoryPool>, typename TConfig = SparseArrayDefaultConfig>
class SparseArray
{
	template<typename, typename, typename>
	friend class SparseArrayIterator;

public:
	// Typedefs
	using Config = TConfig;
	using Value = T;
	using Iterator = SparseArrayIterator<T*, T&, SparseArray*>;
	using ConstIterator = SparseArrayIterator<const T*, const T&, const SparseArray*>;
	using Index = typename Config::Index;

	SparseArray(const TMemoryPool& pool = TMemoryPool())
		: m_pool(pool)
	{
	}

	SparseArray(const Config& config, const TMemoryPool& pool = TMemoryPool())
		: m_pool(pool)
		, m_config(config)
	{
	}

	/// Copy.
	SparseArray(const SparseArray& b)
	{
		*this = b;
	}

	/// Move.
	SparseArray(SparseArray&& b)
	{
		*this = std::move(b);
	}

	/// Destroy.
	~SparseArray()
	{
		destroy();
	}

	/// Copy.
	SparseArray& operator=(const SparseArray& b);

	/// Move operator.
	SparseArray& operator=(SparseArray&& b)
	{
		destroy();

		m_elements = b.m_elements;
		m_metadata = b.m_metadata;
		m_elementCount = b.m_elementCount;
		m_capacity = b.m_capacity;
		m_config = std::move(b.m_config);
		invalidateIterators();

		b.resetMembers();

		return *this;
	}

	/// Get begin.
	Iterator getBegin()
	{
		return Iterator(this, findFirstAlive()
#if ANKI_EXTRA_CHECKS
								  ,
						m_iteratorVer
#endif
		);
	}

	/// Get begin.
	ConstIterator getBegin() const
	{
		return ConstIterator(this, findFirstAlive()
#if ANKI_EXTRA_CHECKS
									   ,
							 m_iteratorVer
#endif
		);
	}

	/// Get end.
	Iterator getEnd()
	{
		return Iterator(this, getMaxNumericLimit<Index>()
#if ANKI_EXTRA_CHECKS
								  ,
						m_iteratorVer
#endif
		);
	}

	/// Get end.
	ConstIterator getEnd() const
	{
		return ConstIterator(this, getMaxNumericLimit<Index>()
#if ANKI_EXTRA_CHECKS
									   ,
							 m_iteratorVer
#endif
		);
	}

	/// Get begin.
	Iterator begin()
	{
		return getBegin();
	}

	/// Get begin.
	ConstIterator begin() const
	{
		return getBegin();
	}

	/// Get end.
	Iterator end()
	{
		return getEnd();
	}

	/// Get end.
	ConstIterator end() const
	{
		return getEnd();
	}

	/// Get the number of elements in the array.
	PtrSize getSize() const
	{
		return m_elementCount;
	}

	/// Return true if it's empty and false otherwise.
	Bool isEmpty() const
	{
		return m_elementCount == 0;
	}

	/// Destroy the array and free its elements.
	void destroy();

	/// Set a value to an index.
	template<typename... TArgs>
	Iterator emplace(Index idx, TArgs&&... args);

	/// Get an iterator.
	Iterator find(Index idx)
	{
		return Iterator(this, findInternal(idx)
#if ANKI_EXTRA_CHECKS
								  ,
						m_iteratorVer
#endif
		);
	}

	/// Get an iterator.
	ConstIterator find(Index idx) const
	{
		return ConstIterator(this, findInternal(idx)
#if ANKI_EXTRA_CHECKS
									   ,
							 m_iteratorVer
#endif
		);
	}

	/// Remove an element.
	void erase(Iterator it);

	/// Check the validity of the array.
	void validate() const;

	const Config& getConfig() const
	{
		return m_config;
	}

protected:
	/// Element metadata.
	class Metadata
	{
	public:
		Index m_idx;
		Bool m_alive;
	};

	TMemoryPool m_pool;
	Value* m_elements = nullptr;
	Metadata* m_metadata = nullptr;
	Index m_elementCount = 0;
	Index m_capacity = 0;
	Config m_config;

#if ANKI_EXTRA_CHECKS
	/// Iterators version. Used to check if iterators point to the newest storage. Needs to be changed whenever we need
	/// to invalidate iterators.
	U32 m_iteratorVer = 0;
#endif

	/// Wrap an index.
	Index mod(const Index idx) const
	{
		ANKI_ASSERT(m_capacity > 0);
		ANKI_ASSERT(isPowerOfTwo(m_capacity));
		return idx & (m_capacity - 1);
	}

	/// Wrap an index.
	static Index mod(const Index idx, Index capacity)
	{
		ANKI_ASSERT(capacity > 0);
		ANKI_ASSERT(isPowerOfTwo(capacity));
		return idx & (capacity - 1);
	}

	F32 calcLoadFactor() const
	{
		ANKI_ASSERT(m_elementCount <= m_capacity);
		ANKI_ASSERT(m_capacity > 0);
		return F32(m_elementCount) / F32(m_capacity);
	}

	/// Insert a value. This method will move the val to a new place.
	/// @return One if the idx was a new element or zero if the idx was there already.
	Index insert(Index idx, Value& val);

	/// Grow the storage and re-insert.
	void grow();

	/// Compute the distance between a desired position and the current one. This method does a trick with capacity to
	/// account for wrapped positions.
	Index distanceFromDesired(const Index crntPos, const Index desiredPos) const
	{
		return mod(crntPos + m_capacity - desiredPos);
	}

	/// Find the first alive element.
	Index findFirstAlive() const
	{
		if(m_elementCount == 0)
		{
			return getMaxNumericLimit<Index>();
		}

		for(Index i = 0; i < m_capacity; ++i)
		{
			if(m_metadata[i].m_alive)
			{
				return i;
			}
		}

		ANKI_ASSERT(0);
		return getMaxNumericLimit<Index>();
	}

	/// Find an element and return its position inside m_elements.
	Index findInternal(Index idx) const;

	/// Reset the class.
	void resetMembers()
	{
		m_elements = nullptr;
		m_metadata = nullptr;
		m_elementCount = 0;
		m_capacity = 0;
		invalidateIterators();
	}

	/// Iterate a number of elements.
	Index iterate(Index pos, Index n) const
	{
		ANKI_ASSERT(pos < m_capacity);
		ANKI_ASSERT(n > 0);
		ANKI_ASSERT(m_metadata[pos].m_alive);

		while(n > 0 && ++pos < m_capacity)
		{
			ANKI_ASSERT(m_metadata[pos].m_alive == 1 || m_metadata[pos].m_alive == 0);
			n -= Index(m_metadata[pos].m_alive);
		}

		return (pos >= m_capacity) ? getMaxNumericLimit<Index>() : pos;
	}

	template<typename... TArgs>
	void emplaceInternal(Index idx, TArgs&&... args);

	void destroyElement(Value& v)
	{
		v.~Value();
#if ANKI_EXTRA_CHECKS
		memset(&v, 0xC, sizeof(v));
#endif
	}

	void invalidateIterators()
	{
#if ANKI_EXTRA_CHECKS
		++m_iteratorVer;
#endif
	}

	F32 getMaxLoadFactor() const
	{
		const F32 f = m_config.getMaxLoadFactor();
		ANKI_ASSERT(f > 0.0f && f < 1.0f);
		return f;
	}

	U32 getLinearProbingCount() const
	{
		const U32 o = m_config.getLinearProbingCount();
		ANKI_ASSERT(o > 0);
		return o;
	}

	Index getInitialStorageSize() const
	{
		const Index o = m_config.getInitialStorageSize();
		ANKI_ASSERT(o > 0);
		return o;
	}
};
/// @}

} // end namespace anki

#include <AnKi/Util/SparseArray.inl.h>
