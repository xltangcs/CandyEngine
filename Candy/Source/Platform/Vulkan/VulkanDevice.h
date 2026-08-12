#pragma once

#include "Runtime/RHI/IR/IRDevice.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <memory>
#include <vector>

namespace Candy {

	class VulkanFunctionLoader;

	// =========================================================================
	// VulkanDevice — Vulkan backend
	//
	// Dynamic loading of vulkan-1.dll; no SDK link library required.
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

		RHICommandQueue& GetCommandQueue() override;
		void WaitIdle() override;

		[[nodiscard]] bool IsInitialized() const { return m_Initialized; }

		// ---- Vulkan native handles -----------------------------------------

		[[nodiscard]] VkInstance       GetVkInstance()       const { return m_Instance; }
		[[nodiscard]] VkPhysicalDevice GetVkPhysicalDevice() const { return m_PhysicalDevice; }
		[[nodiscard]] VkDevice         GetVkDevice()         const { return m_Device; }
		[[nodiscard]] uint32_t         GetGraphicsQueueFamilyIndex() const { return m_GraphicsQueueFamilyIndex; }
		[[nodiscard]] VkCommandPool    GetVkCommandPool() const { return m_CommandPool; }
		[[nodiscard]] VkQueue          GetVkQueue()       const { return m_Queue; }
		[[nodiscard]] PFN_vkVoidFunction GetProcAddr(const char* name) const;

		// ---- Built-in triangle SPIR-V --------------------------------------

		const std::vector<uint32_t>& GetTriangleVSSPIRV();
		const std::vector<uint32_t>& GetTrianglePSSPIRV();

		// ---- Internal device functions (loaded dynamically) ----------------

		PFN_vkCreateBuffer         fnCreateBuffer         = nullptr;
		PFN_vkDestroyBuffer        fnDestroyBuffer        = nullptr;
		PFN_vkAllocateMemory       fnAllocateMemory       = nullptr;
		PFN_vkFreeMemory           fnFreeMemory            = nullptr;
		PFN_vkBindBufferMemory     fnBindBufferMemory     = nullptr;
		PFN_vkMapMemory            fnMapMemory             = nullptr;
		PFN_vkUnmapMemory          fnUnmapMemory           = nullptr;
		PFN_vkCreateCommandPool    fnCreateCommandPool    = nullptr;
		PFN_vkDestroyCommandPool   fnDestroyCommandPool   = nullptr;
		PFN_vkAllocateCommandBuffers fnAllocateCommandBuffers = nullptr;
		PFN_vkFreeCommandBuffers   fnFreeCommandBuffers   = nullptr;
		PFN_vkCreateShaderModule   fnCreateShaderModule   = nullptr;
		PFN_vkDestroyShaderModule  fnDestroyShaderModule  = nullptr;
		PFN_vkCreatePipelineLayout fnCreatePipelineLayout = nullptr;
		PFN_vkDestroyPipelineLayout fnDestroyPipelineLayout = nullptr;
		PFN_vkCreateGraphicsPipelines fnCreateGraphicsPipelines = nullptr;
		PFN_vkDestroyPipeline      fnDestroyPipeline      = nullptr;
		PFN_vkCreateRenderPass     fnCreateRenderPass     = nullptr;
		PFN_vkDestroyRenderPass    fnDestroyRenderPass    = nullptr;
		PFN_vkCreateFramebuffer    fnCreateFramebuffer    = nullptr;
		PFN_vkDestroyFramebuffer   fnDestroyFramebuffer   = nullptr;
		PFN_vkCreateImageView      fnCreateImageView      = nullptr;
		PFN_vkDestroyImageView     fnDestroyImageView     = nullptr;

		// Instance-level extensions
		PFN_vkCreateSwapchainKHR    fnCreateSwapchainKHR    = nullptr;
		PFN_vkDestroySwapchainKHR   fnDestroySwapchainKHR   = nullptr;
		PFN_vkGetSwapchainImagesKHR fnGetSwapchainImagesKHR = nullptr;
		PFN_vkAcquireNextImageKHR   fnAcquireNextImageKHR   = nullptr;
		PFN_vkQueuePresentKHR       fnQueuePresentKHR       = nullptr;
		PFN_vkCreateWin32SurfaceKHR fnCreateWin32SurfaceKHR = nullptr;
		PFN_vkDestroySurfaceKHR     fnDestroySurfaceKHR     = nullptr;

		PFN_vkCreateFence    fnCreateFence    = nullptr;
		PFN_vkDestroyFence   fnDestroyFence   = nullptr;
		PFN_vkWaitForFences  fnWaitForFences  = nullptr;
		PFN_vkResetFences    fnResetFences    = nullptr;

		// ---- Missing functions needed for Texture/Sampler/Descriptor/Renderer2D ---
		PFN_vkCreateImage               fnCreateImage               = nullptr;
		PFN_vkDestroyImage              fnDestroyImage              = nullptr;
		PFN_vkGetImageMemoryRequirements fnGetImageMemoryRequirements = nullptr;
		PFN_vkBindImageMemory           fnBindImageMemory           = nullptr;
		PFN_vkCreateSampler             fnCreateSampler             = nullptr;
		PFN_vkDestroySampler            fnDestroySampler            = nullptr;
		PFN_vkCreateDescriptorSetLayout  fnCreateDescriptorSetLayout  = nullptr;
		PFN_vkDestroyDescriptorSetLayout fnDestroyDescriptorSetLayout = nullptr;
		PFN_vkCreateDescriptorPool      fnCreateDescriptorPool      = nullptr;
		PFN_vkDestroyDescriptorPool     fnDestroyDescriptorPool     = nullptr;
		PFN_vkAllocateDescriptorSets    fnAllocateDescriptorSets    = nullptr;
		PFN_vkUpdateDescriptorSets      fnUpdateDescriptorSets      = nullptr;
		PFN_vkCmdBindDescriptorSets     fnCmdBindDescriptorSets     = nullptr;
		PFN_vkCmdCopyBufferToImage      fnCmdCopyBufferToImage      = nullptr;
		PFN_vkCmdPipelineBarrier        fnCmdPipelineBarrier        = nullptr;
		PFN_vkCreateSemaphore           fnCreateSemaphore           = nullptr;
		PFN_vkDestroySemaphore          fnDestroySemaphore          = nullptr;
		PFN_vkGetBufferMemoryRequirements fnGetBufMemReqs           = nullptr;
		PFN_vkGetPhysicalDeviceMemoryProperties fnGetPhysicalDeviceMemoryProperties = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fnGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fnGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
		PFN_vkQueueWaitIdle                               fnQueueWaitIdle                               = nullptr;
		PFN_vkBeginCommandBuffer        fnBeginCommandBuffer        = nullptr;
		PFN_vkEndCommandBuffer          fnEndCommandBuffer          = nullptr;
		PFN_vkQueueSubmit               fnQueueSubmit               = nullptr;
		PFN_vkCmdDraw                   fnCmdDraw                   = nullptr;
		PFN_vkCmdDrawIndexed            fnCmdDrawIndexed            = nullptr;
		PFN_vkCmdBindVertexBuffers      fnCmdBindVertexBuffers      = nullptr;
		PFN_vkCmdBindIndexBuffer        fnCmdBindIndexBuffer        = nullptr;
		PFN_vkCmdSetViewport            fnCmdSetViewport            = nullptr;
		PFN_vkCmdSetScissor             fnCmdSetScissor             = nullptr;
		PFN_vkCmdBeginRenderPass        fnCmdBeginRenderPass        = nullptr;
		PFN_vkCmdEndRenderPass          fnCmdEndRenderPass          = nullptr;
		PFN_vkCmdBindPipeline           fnCmdBindPipeline           = nullptr;

	private:
		void LoadAllFunctions();

		VkInstance       m_Instance                 = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice           = VK_NULL_HANDLE;
		VkDevice         m_Device                   = VK_NULL_HANDLE;
		uint32_t         m_GraphicsQueueFamilyIndex = UINT32_MAX;
		bool             m_Initialized              = false;
		VkCommandPool    m_CommandPool              = VK_NULL_HANDLE;
		VkQueue          m_Queue                    = VK_NULL_HANDLE;

		std::unique_ptr<VulkanFunctionLoader> m_FunctionLoader;
		Candy::Scope<RHICommandQueue>         m_CommandQueue;

		// Built-in shader cache
		std::vector<uint32_t> m_TriangleVS;
		std::vector<uint32_t> m_TrianglePS;
	};

} // namespace Candy
