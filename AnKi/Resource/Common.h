// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Util/Ptr.h>
#include <AnKi/Gr/Common.h>

namespace anki {

// Forward
class GrManager;
class ResourceManager;
class ResourceFilesystem;
template<typename Type>
class ResourcePointer;
class TransferGpuAllocatorHandle;
class PhysicsWorld;

/// @addtogroup resource
/// @{

#define ANKI_RESOURCE_LOGI(...) ANKI_LOG("RSRC", kNormal, __VA_ARGS__)
#define ANKI_RESOURCE_LOGE(...) ANKI_LOG("RSRC", kError, __VA_ARGS__)
#define ANKI_RESOURCE_LOGW(...) ANKI_LOG("RSRC", kWarning, __VA_ARGS__)
#define ANKI_RESOURCE_LOGF(...) ANKI_LOG("RSRC", kFatal, __VA_ARGS__)
#define ANKI_RESOURCE_LOGV(...) ANKI_LOG("RSRC", kVerbose, __VA_ARGS__)

class ResourceMemoryPool : public HeapMemoryPool, public MakeSingleton<ResourceMemoryPool>
{
	template<typename>
	friend class MakeSingleton;

private:
	ResourceMemoryPool(AllocAlignedCallback allocCb, void* allocCbUserData)
		: HeapMemoryPool(allocCb, allocCbUserData, "ResourceMemPool")
	{
	}

	~ResourceMemoryPool() = default;
};

ANKI_DEFINE_SUBMODULE_UTIL_CONTAINERS(Resource, ResourceMemoryPool)

/// Deleter for ResourcePtr.
template<typename T>
class ResourcePtrDeleter
{
public:
	void operator()(T* ptr);
};

/// Smart pointer for resources.
template<typename T>
using ResourcePtr = IntrusivePtr<T, ResourcePtrDeleter<T>>;

// NOTE: Add resources in 3 places
#define ANKI_INSTANTIATE_RESOURCE(className) \
	class className; \
	using className##Ptr = ResourcePtr<className>;
#include <AnKi/Resource/Resources.def.h>

/// An alias that denotes a ResourceFilesystem path.
using ResourceFilename = CString;
/// @}

} // end namespace anki
