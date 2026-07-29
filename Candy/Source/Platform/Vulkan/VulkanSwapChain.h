#pragma once

#include "Runtime/RHI/RHISwapChain.h"

// Vulkan handle types (no prototypes)
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace Candy {

	// =========================================================================
	// VulkanSwapChain — Vulkan swap chain
	//
	// Wraps VkSwapchainKHR + VkSurfaceKHR + back-buffer VkImages.
	// =========================================================================
	class VulkanSwapChain : public RHISwapChain
	{
	public:
		VulkanSwapChain(const SwapChainDesc& desc);
		virtual ~VulkanSwapChain();

		const SwapChainDesc& GetDesc() const override;

		Candy::Ref<RHITexture> GetCurrentBackBuffer() override;

		void Resize(uint32_t width, uint32_t height) override;

		uint32_t GetWidth()  const override;
		uint32_t GetHeight() const override;

	private:
		SwapChainDesc m_Desc;

		VkSurfaceKHR    m_Surface    = VK_NULL_HANDLE;
		VkSwapchainKHR  m_SwapChain  = VK_NULL_HANDLE;
	};

} // namespace Candy
