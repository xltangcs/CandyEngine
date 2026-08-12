#include "CandyPCH.h"
#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanSwapChain::VulkanSwapChain(VulkanDevice* device, const SwapChainDesc& desc)
		: m_DevicePtr(device), m_Desc(desc)
	{
		m_Extent = { desc.Width, desc.Height };
		CreateSurface();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateFramebuffers();

		CANDY_CORE_INFO("VulkanSwapChain: created {}x{} ({} images)", desc.Width, desc.Height, m_Images.size());
	}

	VulkanSwapChain::~VulkanSwapChain()
	{
		Cleanup();
		CANDY_CORE_INFO("VulkanSwapChain: destroyed");
	}

	void VulkanSwapChain::Cleanup()
	{
		auto dev = m_DevicePtr->GetVkDevice();
		for (auto fb : m_Framebuffers)
			if (fb) m_DevicePtr->fnDestroyFramebuffer(dev, fb, nullptr);
		m_Framebuffers.clear();

		if (m_RenderPass)
			m_DevicePtr->fnDestroyRenderPass(dev, m_RenderPass, nullptr);
		m_RenderPass = VK_NULL_HANDLE;

		for (auto iv : m_ImageViews)
			if (iv) m_DevicePtr->fnDestroyImageView(dev, iv, nullptr);
		m_ImageViews.clear();
		m_Images.clear();

		if (m_SwapChain)
			m_DevicePtr->fnDestroySwapchainKHR(dev, m_SwapChain, nullptr);
		m_SwapChain = VK_NULL_HANDLE;

		if (m_Surface)
			m_DevicePtr->fnDestroySurfaceKHR(m_DevicePtr->GetVkInstance(), m_Surface, nullptr);
		m_Surface = VK_NULL_HANDLE;
	}

	void VulkanSwapChain::CreateSurface()
	{
		VkWin32SurfaceCreateInfoKHR surfaceCI = {};
		surfaceCI.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCI.hinstance = GetModuleHandleA(nullptr);
		surfaceCI.hwnd      = static_cast<HWND>(m_Desc.Window.Native);

		if (m_DevicePtr->fnCreateWin32SurfaceKHR(
			m_DevicePtr->GetVkInstance(), &surfaceCI, nullptr, &m_Surface) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanSwapChain: vkCreateWin32SurfaceKHR failed");
		}
	}

	void VulkanSwapChain::CreateSwapChain()
	{
		auto physDev = m_DevicePtr->GetVkPhysicalDevice();
		auto dev     = m_DevicePtr->GetVkDevice();

		// Choose surface format
		uint32_t formatCount;
		m_DevicePtr->fnGetPhysicalDeviceSurfaceFormatsKHR(physDev, m_Surface, &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		m_DevicePtr->fnGetPhysicalDeviceSurfaceFormatsKHR(physDev, m_Surface, &formatCount, formats.data());

		m_Format = formats[0].format;
		for (auto& f : formats)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{ m_Format = f.format; break; }
		}

		VkSurfaceCapabilitiesKHR caps;
		m_DevicePtr->fnGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, m_Surface, &caps);

		m_Extent.width  = std::clamp(m_Extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
		m_Extent.height = std::clamp(m_Extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

		uint32_t imageCount = std::max(m_Desc.BufferCount, caps.minImageCount);
		if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

		VkSwapchainCreateInfoKHR swapCI = {};
		swapCI.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapCI.surface          = m_Surface;
		swapCI.minImageCount    = imageCount;
		swapCI.imageFormat      = m_Format;
		swapCI.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		swapCI.imageExtent      = m_Extent;
		swapCI.imageArrayLayers = 1;
		swapCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapCI.preTransform     = caps.currentTransform;
		swapCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapCI.presentMode      = m_Desc.VSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
		swapCI.clipped          = VK_TRUE;

		if (m_DevicePtr->fnCreateSwapchainKHR(dev, &swapCI, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanSwapChain: vkCreateSwapchainKHR failed");
			return;
		}

		uint32_t count;
		m_DevicePtr->fnGetSwapchainImagesKHR(dev, m_SwapChain, &count, nullptr);
		m_Images.resize(count);
		m_DevicePtr->fnGetSwapchainImagesKHR(dev, m_SwapChain, &count, m_Images.data());
	}

	void VulkanSwapChain::CreateImageViews()
	{
		auto dev = m_DevicePtr->GetVkDevice();
		m_ImageViews.resize(m_Images.size());

		for (size_t i = 0; i < m_Images.size(); ++i)
		{
			VkImageViewCreateInfo viewCI = {};
			viewCI.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCI.image      = m_Images[i];
			viewCI.viewType   = VK_IMAGE_VIEW_TYPE_2D;
			viewCI.format     = m_Format;
			viewCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCI.subresourceRange.baseMipLevel   = 0;
			viewCI.subresourceRange.levelCount     = 1;
			viewCI.subresourceRange.baseArrayLayer = 0;
			viewCI.subresourceRange.layerCount     = 1;

			if (m_DevicePtr->fnCreateImageView(dev, &viewCI, nullptr, &m_ImageViews[i]) != VK_SUCCESS)
				CANDY_CORE_ERROR("VulkanSwapChain: CreateImageView({}) failed", i);
		}
	}

	void VulkanSwapChain::CreateRenderPass()
	{
		VkAttachmentDescription colorAttach = {};
		colorAttach.format         = m_Format;
		colorAttach.samples        = VK_SAMPLE_COUNT_1_BIT;
		colorAttach.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttach.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttach.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttach.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttach.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments    = &colorRef;

		VkSubpassDependency dep = {};
		dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass    = 0;
		dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo rpCI = {};
		rpCI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpCI.attachmentCount = 1;
		rpCI.pAttachments    = &colorAttach;
		rpCI.subpassCount    = 1;
		rpCI.pSubpasses      = &subpass;
		rpCI.dependencyCount = 1;
		rpCI.pDependencies   = &dep;

		m_DevicePtr->fnCreateRenderPass(m_DevicePtr->GetVkDevice(), &rpCI, nullptr, &m_RenderPass);
	}

	void VulkanSwapChain::CreateFramebuffers()
	{
		auto dev = m_DevicePtr->GetVkDevice();
		m_Framebuffers.resize(m_ImageViews.size());

		for (size_t i = 0; i < m_ImageViews.size(); ++i)
		{
			VkFramebufferCreateInfo fbCI = {};
			fbCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbCI.renderPass      = m_RenderPass;
			fbCI.attachmentCount = 1;
			fbCI.pAttachments    = &m_ImageViews[i];
			fbCI.width           = m_Extent.width;
			fbCI.height          = m_Extent.height;
			fbCI.layers          = 1;

			m_DevicePtr->fnCreateFramebuffer(dev, &fbCI, nullptr, &m_Framebuffers[i]);
		}
	}

	const SwapChainDesc& VulkanSwapChain::GetDesc() const { return m_Desc; }

	Ref<RHITexture> VulkanSwapChain::GetCurrentBackBuffer()
	{
		CANDY_CORE_WARN("TODO: VulkanSwapChain::GetCurrentBackBuffer — texture wrapper needed");
		return nullptr;
	}

	uint32_t VulkanSwapChain::AcquireNextImage(VkSemaphore signalSem, VkFence fence)
	{
		m_DevicePtr->fnAcquireNextImageKHR(
			m_DevicePtr->GetVkDevice(), m_SwapChain, UINT64_MAX,
			signalSem, fence, &m_CurrentImageIndex);
		return m_CurrentImageIndex;
	}

	VkFramebuffer VulkanSwapChain::GetFramebuffer(uint32_t index) const
	{
		return (index < m_Framebuffers.size()) ? m_Framebuffers[index] : VK_NULL_HANDLE;
	}

	void VulkanSwapChain::Resize(uint32_t width, uint32_t height)
	{
		m_Desc.Width = width; m_Desc.Height = height;
		Cleanup();
		m_Extent = { width, height };
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateFramebuffers();
		CANDY_CORE_INFO("VulkanSwapChain::Resize {}x{}", width, height);
	}

	uint32_t VulkanSwapChain::GetWidth()  const { return m_Desc.Width; }
	uint32_t VulkanSwapChain::GetHeight() const { return m_Desc.Height; }

} // namespace Candy
