// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Resource/TransferGpuAllocator.h>
#include <AnKi/Resource/ResourceFilesystem.h>
#include <AnKi/Util/List.h>
#include <AnKi/Util/Functions.h>
#include <AnKi/Util/String.h>

namespace anki {

// Forward
class GrManager;
class PhysicsWorld;
class ResourceManager;
class AsyncLoader;
class ResourceManagerModel;
class ShaderCompilerCache;
class ShaderProgramResourceSystem;

/// @addtogroup resource
/// @{

inline NumericCVar<PtrSize> g_transferScratchMemorySizeCVar("Rsrc", "TransferScratchMemorySize", 256_MB, 1_MB, 4_GB,
															"Memory that is used fot texture and buffer uploads");

/// Manage resources of a certain type
template<typename Type>
class TypeResourceManager
{
protected:
	TypeResourceManager()
	{
	}

	~TypeResourceManager()
	{
		ANKI_ASSERT(m_ptrs.isEmpty() && "Forgot to delete some resources");
		m_ptrs.destroy();
	}

	Type* findLoadedResource(const CString& filename)
	{
		auto it = find(filename);
		return (it != m_ptrs.end()) ? *it : nullptr;
	}

	void registerResource(Type* ptr)
	{
		ANKI_ASSERT(find(ptr->getFilename()) == m_ptrs.getEnd());
		m_ptrs.pushBack(ptr);
	}

	void unregisterResource(Type* ptr)
	{
		auto it = find(ptr->getFilename());
		ANKI_ASSERT(it != m_ptrs.end());
		m_ptrs.erase(it);
	}

private:
	using Container = ResourceList<Type*>;

	Container m_ptrs;

	typename Container::Iterator find(const CString& filename)
	{
		typename Container::Iterator it;

		for(it = m_ptrs.getBegin(); it != m_ptrs.getEnd(); ++it)
		{
			if((*it)->getFilename() == filename)
			{
				break;
			}
		}

		return it;
	}
};

/// Resource manager. It holds a few global variables
class ResourceManager : public MakeSingleton<ResourceManager>,

#define ANKI_INSTANTIATE_RESOURCE(rsrc_, ptr_) \
public \
	TypeResourceManager<rsrc_>

#define ANKI_INSTANSIATE_RESOURCE_DELIMITER() ,

#include <AnKi/Resource/InstantiationMacros.h>
#undef ANKI_INSTANTIATE_RESOURCE
#undef ANKI_INSTANSIATE_RESOURCE_DELIMITER

{
	template<typename T>
	friend class ResourcePtrDeleter;
	friend class ResourceObject;

	template<typename>
	friend class MakeSingleton;

public:
	Error init(AllocAlignedCallback allocCallback, void* allocCallbackData);

	/// Load a resource.
	template<typename T>
	Error loadResource(const CString& filename, ResourcePtr<T>& out, Bool async = true);

	// Internals:

	ANKI_INTERNAL TransferGpuAllocator& getTransferGpuAllocator()
	{
		return *m_transferGpuAlloc;
	}

	template<typename T>
	ANKI_INTERNAL T* findLoadedResource(const CString& filename)
	{
		return TypeResourceManager<T>::findLoadedResource(filename);
	}

	template<typename T>
	ANKI_INTERNAL void registerResource(T* ptr)
	{
		TypeResourceManager<T>::registerResource(ptr);
	}

	template<typename T>
	ANKI_INTERNAL void unregisterResource(T* ptr)
	{
		TypeResourceManager<T>::unregisterResource(ptr);
	}

	ANKI_INTERNAL AsyncLoader& getAsyncLoader()
	{
		return *m_asyncLoader;
	}

	/// Get the total number of completed async tasks.
	ANKI_INTERNAL U64 getAsyncTaskCompletedCount() const;

	/// Return the container of program libraries.
	ANKI_INTERNAL const ShaderProgramResourceSystem& getShaderProgramResourceSystem() const
	{
		return *m_shaderProgramSystem;
	}

	ANKI_INTERNAL ResourceFilesystem& getFilesystem()
	{
		return *m_fs;
	}

private:
	ResourceFilesystem* m_fs = nullptr;
	AsyncLoader* m_asyncLoader = nullptr; ///< Async loading thread
	ShaderProgramResourceSystem* m_shaderProgramSystem = nullptr;
	TransferGpuAllocator* m_transferGpuAlloc = nullptr;

	U64 m_uuid = 0;

	ResourceManager();

	~ResourceManager();
};
/// @}

} // end namespace anki
