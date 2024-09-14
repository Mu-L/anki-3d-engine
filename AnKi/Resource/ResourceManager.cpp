// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Resource/ResourceManager.h>
#include <AnKi/Resource/AsyncLoader.h>
#include <AnKi/Resource/ShaderProgramResourceSystem.h>
#include <AnKi/Resource/AnimationResource.h>
#include <AnKi/Util/Logger.h>
#include <AnKi/Util/CVarSet.h>

#include <AnKi/Resource/MaterialResource.h>
#include <AnKi/Resource/MeshResource.h>
#include <AnKi/Resource/CpuMeshResource.h>
#include <AnKi/Resource/ModelResource.h>
#include <AnKi/Resource/ScriptResource.h>
#include <AnKi/Resource/DummyResource.h>
#include <AnKi/Resource/ParticleEmitterResource.h>
#include <AnKi/Resource/ImageResource.h>
#include <AnKi/Resource/GenericResource.h>
#include <AnKi/Resource/ImageAtlasResource.h>
#include <AnKi/Resource/ShaderProgramResource.h>
#include <AnKi/Resource/SkeletonResource.h>

namespace anki {

ResourceManager::ResourceManager()
{
}

ResourceManager::~ResourceManager()
{
	ANKI_RESOURCE_LOGI("Destroying resource manager");

	deleteInstance(ResourceMemoryPool::getSingleton(), m_asyncLoader);
	deleteInstance(ResourceMemoryPool::getSingleton(), m_shaderProgramSystem);
	deleteInstance(ResourceMemoryPool::getSingleton(), m_transferGpuAlloc);
	deleteInstance(ResourceMemoryPool::getSingleton(), m_fs);

	ResourceMemoryPool::freeSingleton();
}

Error ResourceManager::init(AllocAlignedCallback allocCallback, void* allocCallbackData)
{
	ANKI_RESOURCE_LOGI("Initializing resource manager");

	ResourceMemoryPool::allocateSingleton(allocCallback, allocCallbackData);

	m_fs = newInstance<ResourceFilesystem>(ResourceMemoryPool::getSingleton());
	ANKI_CHECK(m_fs->init());

	// Init the thread
	m_asyncLoader = newInstance<AsyncLoader>(ResourceMemoryPool::getSingleton());

	m_transferGpuAlloc = newInstance<TransferGpuAllocator>(ResourceMemoryPool::getSingleton());
	ANKI_CHECK(m_transferGpuAlloc->init(g_transferScratchMemorySizeCVar));

	// Init the programs
	m_shaderProgramSystem = newInstance<ShaderProgramResourceSystem>(ResourceMemoryPool::getSingleton());
	ANKI_CHECK(m_shaderProgramSystem->init());

	return Error::kNone;
}

template<typename T>
Error ResourceManager::loadResource(const CString& filename, ResourcePtr<T>& out, Bool async)
{
	ANKI_ASSERT(!out.isCreated() && "Already loaded");

	Error err = Error::kNone;

	T* const other = findLoadedResource<T>(filename);

	if(other)
	{
		// Found
		out.reset(other);
	}
	else
	{
		// Allocate ptr
		T* ptr = newInstance<T>(ResourceMemoryPool::getSingleton());
		ANKI_ASSERT(ptr->getRefcount() == 0);

		// Increment the refcount in that case where async jobs increment it and decrement it in the scope of a load()
		ptr->retain();

		err = ptr->load(filename, async);
		if(err)
		{
			ANKI_RESOURCE_LOGE("Failed to load resource: %s", &filename[0]);
			deleteInstance(ResourceMemoryPool::getSingleton(), ptr);
			return err;
		}

		ptr->setFilename(filename);
		ptr->setUuid(++m_uuid);

		// Register resource
		registerResource(ptr);
		out.reset(ptr);

		// Decrement because of the increment happened a few lines above
		ptr->release();
	}

	return err;
}

// Instansiate the ResourceManager::loadResource()
#define ANKI_INSTANTIATE_RESOURCE(rsrc_, ptr_) \
	template Error ResourceManager::loadResource<rsrc_>(const CString& filename, ResourcePtr<rsrc_>& out, Bool async);
#define ANKI_INSTANSIATE_RESOURCE_DELIMITER()
#include <AnKi/Resource/InstantiationMacros.h>
#undef ANKI_INSTANTIATE_RESOURCE
#undef ANKI_INSTANSIATE_RESOURCE_DELIMITER

} // end namespace anki
