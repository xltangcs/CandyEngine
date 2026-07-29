#pragma once

#include "Runtime/RHI/RHISwapChain.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <vector>

namespace Candy {

	class VulkanDevice;

	class VulkanSwapChain : public RHISwapChain
	{
	public:
		VulkanSwapChain(VulkanDevice* device, const SwapChainDesc& desc);
		virtual ~VulkanSwapChain();

		const SwapChainDesc& GetDesc() const override;
		Candy::Ref<RHITexture> GetCurrentBackBuffer() override;
		void Resize(uint32_t width, uint32_t height) override;
		uint32_t GetWidth()  const override;
		uint32_t GetHeight() const override;

		[[nodiscard]] VkSwapchainKHR GetVkSwapchain() const { return m_SwapChain; }
		[[nodiscard]] VkRenderPass   GetRenderPass()  const { return m_RenderPass; }
		[[nodiscard]] VkFramebuffer  GetFramebuffer(uint32_t index) const;
		[[nodiscard]] uint32_t       GetImageCount()  const { return static_cast<uint32_t>(m_Images.size()); }
		uint32_t AcquireNextImage(VkSemaphore signalSem, VkFence fence);

	private:
		void CreateSurface();
		void CreateSwapChain();
		void CreateImageViews();
		void CreateRenderPass();
		void CreateFramebuffers();
		void Cleanup();

		VulkanDevice*  m_DevicePtr = nullptr;
		SwapChainDesc  m_Desc;
		VkSurfaceKHR   m_Surface   = VK_NULL_HANDLE;
		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
		VkRenderPass   m_RenderPass = VK_NULL_HANDLE;

		std::vector<VkImage>       m_Images;
		std::vector<VkImageView>   m_ImageViews;
		std::vector<VkFramebuffer> m_Framebuffers;

		VkFormat       m_Format    = VK_FORMAT_B8G8R8A8_UNORM;
		VkExtent2D     m_Extent    = {};
		uint32_t       m_CurrentImageIndex = 0;
	};

} // namespace Candy
