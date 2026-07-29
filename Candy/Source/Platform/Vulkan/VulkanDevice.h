#pragma once

#include "Runtime/RHI/IR/IRDevice.h"

// Vulkan handle types (no prototypes — we use dynamic loading)
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <memory>

namespace Candy {

	// Forward declaration — defined in VulkanDevice.cpp
	class VulkanFunctionLoader;

	// =========================================================================
	// VulkanDevice — Vulkan backend implementation
	//
	// Inherits from IR::IRDevice.  Uses dynamic loading of vulkan-1.dll so
	// that no Vulkan SDK link library is required at build time.
	// =========================================================================
	class VulkanDevice : public IR::IRDevice
	{
	public:
		VulkanDevice();
		virtual ~VulkanDevice();

		// ---- Resource creation ----------------------------------------------

		Candy::Ref<RHIBuffer>   CreateBuffer(const BufferDesc& desc) override;
		Candy::Ref<RHITexture>  CreateTexture(const TextureDesc& desc) override;
		Candy::Ref<RHISampler>  CreateSampler(const SamplerDesc& desc) override;

		Candy::Ref<RHIShaderModule> CreateShaderModule(const void* spirvBytecode, uint32_t byteSize, const std::string& debugName = "") override;

		Candy::Ref<RHIGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const Candy::Ref<RHIShaderModule>& vs, const Candy::Ref<RHIShaderModule>& fs) override;

		Candy::Ref<RHISwapChain> CreateSwapChain(const SwapChainDesc& desc) override;

		// ---- Command submission --------------------------------------------

		RHICommandQueue& GetCommandQueue() override;

		// ---- Query ---------------------------------------------------------

		void WaitIdle() override;

		[[nodiscard]] bool IsInitialized() const { return m_Initialized; }

		// ---- Internal Vulkan function wrappers (loaded dynamically) --------

		VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pCount, VkPhysicalDevice* pDevices);
		void     vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
		void     vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkQueueFamilyProperties* pProperties);
		VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
		void     vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
		void     vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator);
		void     vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);
		VkResult vkQueueWaitIdle(VkQueue queue);
		VkResult vkDeviceWaitIdle(VkDevice device);

	private:
		void LoadInstanceFunctions();
		void LoadDeviceFunctions();

		// ---- Vulkan handles -------------------------------------------------

		VkInstance       m_Instance                 = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice           = VK_NULL_HANDLE;
		VkDevice         m_Device                   = VK_NULL_HANDLE;
		uint32_t         m_GraphicsQueueFamilyIndex = UINT32_MAX;
		bool             m_Initialized              = false;

		// ---- Loader & queue -------------------------------------------------

		std::unique_ptr<VulkanFunctionLoader> m_FunctionLoader;
		Candy::Scope<RHICommandQueue>         m_CommandQueue;

		// ---- Dynamically-loaded function pointers ---------------------------

		struct DeviceFunctions
		{
			PFN_vkGetPhysicalDeviceProperties            vkGetPhysicalDeviceProperties            = nullptr;
			PFN_vkEnumeratePhysicalDevices               vkEnumeratePhysicalDevices               = nullptr;
			PFN_vkGetPhysicalDeviceQueueFamilyProperties  vkGetPhysicalDeviceQueueFamilyProperties  = nullptr;
			PFN_vkCreateDevice                           vkCreateDevice                           = nullptr;
			PFN_vkGetDeviceQueue                         vkGetDeviceQueue                         = nullptr;
			PFN_vkDestroyInstance                        vkDestroyInstance                        = nullptr;
			PFN_vkDestroyDevice                          vkDestroyDevice                          = nullptr;
			PFN_vkQueueWaitIdle                          vkQueueWaitIdle                          = nullptr;
			PFN_vkDeviceWaitIdle                         vkDeviceWaitIdle                         = nullptr;
			PFN_vkGetDeviceProcAddr                      vkDevGetProcAddr                         = nullptr;
		};
		DeviceFunctions m_Funcs;
	};

} // namespace Candy
