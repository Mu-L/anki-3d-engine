// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Gr/GrManager.h>
#include <AnKi/Gr/Vulkan/VkCommon.h>
#include <AnKi/Gr/Vulkan/VkSemaphoreFactory.h>
#include <AnKi/Gr/Vulkan/VkFenceFactory.h>
#include <AnKi/Gr/Vulkan/VkSwapchainFactory.h>
#include <AnKi/Gr/Vulkan/VkFrameGarbageCollector.h>
#include <AnKi/Util/File.h>

namespace anki {

/// @note Disable that because it crashes Intel drivers
#define ANKI_GR_MANAGER_DEBUG_MEMMORY ANKI_EXTRA_CHECKS && 0

// Forward
class TextureFallbackUploader;
class MicroCommandBuffer;

/// @addtogroup vulkan
/// @{

enum class AsyncComputeType
{
	kProper,
	kLowPriorityQueue,
	kDisabled
};

/// Vulkan implementation of GrManager.
class GrManagerImpl : public GrManager
{
	friend class GrManager;

public:
	GrManagerImpl()
	{
	}

	~GrManagerImpl();

	Error init(const GrManagerInitInfo& cfg);

	ConstWeakArray<U32> getQueueFamilies() const
	{
		const Bool hasAsyncCompute = m_queueFamilyIndices[GpuQueueType::kGeneral] != m_queueFamilyIndices[GpuQueueType::kCompute];
		return (hasAsyncCompute) ? m_queueFamilyIndices : ConstWeakArray<U32>(&m_queueFamilyIndices[0], 1);
	}

	AsyncComputeType getAsyncComputeType() const
	{
		if(m_queues[GpuQueueType::kCompute] == nullptr)
		{
			return AsyncComputeType::kDisabled;
		}
		else if(m_queueFamilyIndices[GpuQueueType::kCompute] == m_queueFamilyIndices[GpuQueueType::kGeneral])
		{
			return AsyncComputeType::kLowPriorityQueue;
		}
		else
		{
			return AsyncComputeType::kProper;
		}
	}

	const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const
	{
		return m_devProps.properties;
	}

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& getPhysicalDeviceRayTracingProperties() const
	{
		return m_rtPipelineProps;
	}

	TexturePtr acquireNextPresentableTexture();

	void endFrame();

	void finish();

	VkDevice getDevice() const
	{
		ANKI_ASSERT(m_device);
		return m_device;
	}

	VkPhysicalDevice getPhysicalDevice() const
	{
		ANKI_ASSERT(m_physicalDevice);
		return m_physicalDevice;
	}

	VkInstance getInstance() const
	{
		ANKI_ASSERT(m_instance);
		return m_instance;
	}

	const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const
	{
		return m_memoryProperties;
	}

	VulkanExtensions getExtensions() const
	{
		return m_extensions;
	}

	MicroSwapchainPtr getSwapchain() const
	{
		return m_crntSwapchain;
	}

	VkSurfaceKHR getSurface() const
	{
		ANKI_ASSERT(m_surface);
		return m_surface;
	}

	void getNativeWindowSize(U32& width, U32& height) const
	{
		width = m_nativeWindowWidth;
		height = m_nativeWindowHeight;
		ANKI_ASSERT(width && height);
	}

	/// @name Debug report
	/// @{
	void trySetVulkanHandleName(CString name, VkObjectType type, U64 handle) const;

	void trySetVulkanHandleName(CString name, VkObjectType type, void* handle) const
	{
		trySetVulkanHandleName(name, type, U64(ptrToNumber(handle)));
	}
	/// @}

	/// @note It's thread-safe.
	void printPipelineShaderInfo(VkPipeline ppline, CString name, U64 hash = 0) const;

	FrameGarbageCollector& getFrameGarbageCollector()
	{
		return m_frameGarbageCollector;
	}

private:
	U64 m_frame = 0;

#if ANKI_GR_MANAGER_DEBUG_MEMMORY
	VkAllocationCallbacks m_debugAllocCbs;
	static constexpr U32 MAX_ALLOC_ALIGNMENT = 64;
	static constexpr PtrSize ALLOC_SIG = 0xF00B00;

	struct alignas(MAX_ALLOC_ALIGNMENT) AllocHeader
	{
		PtrSize m_sig;
		PtrSize m_size;
	};
#endif

	VkInstance m_instance = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VulkanExtensions m_extensions = VulkanExtensions::kNone;
	VkDevice m_device = VK_NULL_HANDLE;
	VulkanQueueFamilies m_queueFamilyIndices = {kMaxU32, kMaxU32};
	Array<VkQueue, U32(GpuQueueType::kCount)> m_queues = {nullptr, nullptr};
	Mutex m_globalMtx;

	VkPhysicalDeviceProperties2 m_devProps = {};
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtPipelineProps = {};

	VkDebugUtilsMessengerEXT m_debugUtilsMessager = VK_NULL_HANDLE;

	mutable SpinLock m_shaderStatsMtx;

	/// @name Surface_related
	/// @{
	class PerFrame
	{
	public:
		MicroFencePtr m_presentFence;
		MicroSemaphorePtr m_acquireSemaphore;

		/// Signaled by the submit that renders to the default FB. Present waits for it.
		MicroSemaphorePtr m_renderSemaphore;

		GpuQueueType m_queueWroteToSwapchainImage = GpuQueueType::kCount;
	};

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	U32 m_nativeWindowWidth = 0;
	U32 m_nativeWindowHeight = 0;
	MicroSwapchainPtr m_crntSwapchain;
	U8 m_acquiredImageIdx = kMaxU8;

	Array<PerFrame, kMaxFramesInFlight> m_perFrame;
	/// @}

	VkPhysicalDeviceMemoryProperties m_memoryProperties;

	FrameGarbageCollector m_frameGarbageCollector;

	Error initInternal(const GrManagerInitInfo& init);
	Error initInstance();
	Error initSurface();
	Error initDevice();
	Error initMemory();

#if ANKI_GR_MANAGER_DEBUG_MEMMORY
	static void* allocateCallback(void* userData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

	static void* reallocateCallback(void* userData, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

	static void freeCallback(void* userData, void* ptr);
#endif

	void resetFrame(PerFrame& frame);

	static VkBool32 debugReportCallbackEXT(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
										   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	Error printPipelineShaderInfoInternal(VkPipeline ppline, CString name, U64 hash) const;

	template<typename TProps>
	void getPhysicalDeviceProperties2(TProps& props) const
	{
		VkPhysicalDeviceProperties2 properties = {};
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &props;
		vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties);
	}

	template<typename TFeatures>
	void getPhysicalDevicaFeatures2(TFeatures& features) const
	{
		VkPhysicalDeviceFeatures2 features2 = {};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &features;
		vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);
	}
};
/// @}

} // end namespace anki
