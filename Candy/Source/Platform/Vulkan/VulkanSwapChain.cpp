#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanSwapChain::VulkanSwapChain(const SwapChainDesc& desc)
		: m_Desc(desc)
	{
		CANDY_CORE_INFO("VulkanSwapChain: created {}x{} (buffer count: {}, vsync: {})",
		                desc.Width, desc.Height, desc.BufferCount, desc.VSync);
	}

	VulkanSwapChain::~VulkanSwapChain()
	{
		if (m_SwapChain)
		{
			// TODO: vkDestroySwapchainKHR via device function pointer
			m_SwapChain = VK_NULL_HANDLE;
		}
		if (m_Surface)
		{
			// TODO: vkDestroySurfaceKHR via instance function pointer
			m_Surface = VK_NULL_HANDLE;
		}
		CANDY_CORE_INFO("VulkanSwapChain: destroyed");
	}

	const SwapChainDesc& VulkanSwapChain::GetDesc() const
	{
		return m_Desc;
	}

	Ref<RHITexture> VulkanSwapChain::GetCurrentBackBuffer()
	{
		CANDY_CORE_WARN("TODO: VulkanSwapChain::GetCurrentBackBuffer — not yet implemented");
		return nullptr;
	}

	void VulkanSwapChain::Resize(uint32_t width, uint32_t height)
	{
		m_Desc.Width  = width;
		m_Desc.Height = height;
		CANDY_CORE_INFO("VulkanSwapChain::Resize {}x{}", width, height);
		CANDY_CORE_WARN("TODO: VulkanSwapChain::Resize — actual swapchain recreation not yet implemented");
	}

	uint32_t VulkanSwapChain::GetWidth() const
	{
		return m_Desc.Width;
	}

	uint32_t VulkanSwapChain::GetHeight() const
	{
		return m_Desc.Height;
	}

} // namespace Candy
