#include "CandyPCH.h"
#include <Windows.h>
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/Core/Log.h"

#include <algorithm>

namespace Candy {

	static uint32_t FindMemType(VulkanDevice* dev, uint32_t typeFilter, VkMemoryPropertyFlags props)
	{
		VkPhysicalDeviceMemoryProperties memProps;
		dev->fnGetPhysicalDeviceMemoryProperties(dev->GetVkPhysicalDevice(), &memProps);
		for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
			if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
				return i;
		CANDY_CORE_ASSERT(false, "VulkanFramebuffer: no suitable memory type");
		return 0;
	}

	VulkanFramebuffer::VulkanFramebuffer(const FramebufferDesc& desc, VulkanDevice* device)
		: m_Desc(desc), m_Device(device)
	{
		Invalidate();
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		VkDevice dev = m_Device ? m_Device->GetVkDevice() : VK_NULL_HANDLE;
		if (!dev) return;

		m_Device->WaitIdle();

		if (m_Framebuffer) m_Device->fnDestroyFramebuffer(dev, m_Framebuffer, nullptr);
		if (m_RenderPass)  m_Device->fnDestroyRenderPass(dev, m_RenderPass, nullptr);

		for (auto& v : m_ColorViews)  m_Device->fnDestroyImageView(dev, v, nullptr);
		for (auto& v : m_ColorImages) m_Device->fnDestroyImage(dev, v, nullptr);
		for (auto& v : m_ColorMemories) m_Device->fnFreeMemory(dev, v, nullptr);

		if (m_DepthView)   m_Device->fnDestroyImageView(dev, m_DepthView, nullptr);
		if (m_DepthImage)  m_Device->fnDestroyImage(dev, m_DepthImage, nullptr);
		if (m_DepthMemory) m_Device->fnFreeMemory(dev, m_DepthMemory, nullptr);

		CANDY_CORE_INFO("VulkanFramebuffer: destroyed");
	}

	VkFormat VulkanFramebuffer::MapFormat(RHIFormat fmt) const
	{
		switch (fmt)
		{
		case RHIFormat::R8G8B8A8Unorm:  return VK_FORMAT_R8G8B8A8_UNORM;
		case RHIFormat::R32Sint:        return VK_FORMAT_R32_SINT;
		case RHIFormat::D24UnormS8Uint: return VK_FORMAT_D24_UNORM_S8_UINT;
		default:                        return VK_FORMAT_R8G8B8A8_UNORM;
		}
	}

	void VulkanFramebuffer::Invalidate()
	{
		if (m_Desc.SwapChainTarget) return;

		VkDevice dev = m_Device->GetVkDevice();
		uint32_t w = m_Desc.Width, h = m_Desc.Height;
		uint32_t colorCount = static_cast<uint32_t>(m_Desc.ColorAttachments.size());

		// --- Create color attachment images + views ---
		m_ColorImages.resize(colorCount);
		m_ColorMemories.resize(colorCount);
		m_ColorViews.resize(colorCount);

		for (uint32_t i = 0; i < colorCount; ++i)
		{
			VkFormat fmt = MapFormat(m_Desc.ColorAttachments[i].Format);

			VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			ici.imageType     = VK_IMAGE_TYPE_2D;
			ici.format        = fmt;
			ici.extent        = { w, h, 1 };
			ici.mipLevels     = 1;
			ici.arrayLayers   = 1;
			ici.samples       = VK_SAMPLE_COUNT_1_BIT;
			ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
			ici.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			m_Device->fnCreateImage(dev, &ici, nullptr, &m_ColorImages[i]);

			VkMemoryRequirements mr;
			m_Device->fnGetImageMemoryRequirements(dev, m_ColorImages[i], &mr);
			uint32_t memType = FindMemType(m_Device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize  = mr.size;
			ai.memoryTypeIndex = memType;
			m_Device->fnAllocateMemory(dev, &ai, nullptr, &m_ColorMemories[i]);
			m_Device->fnBindImageMemory(dev, m_ColorImages[i], m_ColorMemories[i], 0);

			VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			ivci.image    = m_ColorImages[i];
			ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			ivci.format   = fmt;
			ivci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			m_Device->fnCreateImageView(dev, &ivci, nullptr, &m_ColorViews[i]);
		}

		// --- Depth attachment ---
		if (m_Desc.HasDepthStencil)
		{
			VkFormat fmt = MapFormat(m_Desc.DepthStencilAttachment.Format);

			VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			ici.imageType     = VK_IMAGE_TYPE_2D;
			ici.format        = fmt;
			ici.extent        = { w, h, 1 };
			ici.mipLevels     = 1;
			ici.arrayLayers   = 1;
			ici.samples       = VK_SAMPLE_COUNT_1_BIT;
			ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
			ici.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			m_Device->fnCreateImage(dev, &ici, nullptr, &m_DepthImage);

			VkMemoryRequirements mr;
			m_Device->fnGetImageMemoryRequirements(dev, m_DepthImage, &mr);
			uint32_t memType = FindMemType(m_Device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize  = mr.size;
			ai.memoryTypeIndex = memType;
			m_Device->fnAllocateMemory(dev, &ai, nullptr, &m_DepthMemory);
			m_Device->fnBindImageMemory(dev, m_DepthImage, m_DepthMemory, 0);

			VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			ivci.image    = m_DepthImage;
			ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			ivci.format   = fmt;
			ivci.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
			m_Device->fnCreateImageView(dev, &ivci, nullptr, &m_DepthView);
		}

		// --- Render pass ---
		std::vector<VkAttachmentDescription> attachments;
		std::vector<VkAttachmentReference>   colorRefs;
		VkAttachmentReference depthRef = {};

		for (uint32_t i = 0; i < colorCount; ++i)
		{
			VkAttachmentDescription att = {};
			att.format         = MapFormat(m_Desc.ColorAttachments[i].Format);
			att.samples        = VK_SAMPLE_COUNT_1_BIT;
			att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
			att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
			att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
			att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachments.push_back(att);

			colorRefs.push_back({ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		}

		if (m_DepthView != VK_NULL_HANDLE)
		{
			VkAttachmentDescription att = {};
			att.format         = MapFormat(m_Desc.DepthStencilAttachment.Format);
			att.samples        = VK_SAMPLE_COUNT_1_BIT;
			att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
			att.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
			att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			depthRef.attachment = static_cast<uint32_t>(attachments.size());
			depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(att);
		}

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
		subpass.pColorAttachments       = colorRefs.data();
		subpass.pDepthStencilAttachment = (m_DepthView != VK_NULL_HANDLE) ? &depthRef : nullptr;

		VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rpci.attachmentCount = static_cast<uint32_t>(attachments.size());
		rpci.pAttachments    = attachments.data();
		rpci.subpassCount    = 1;
		rpci.pSubpasses      = &subpass;

		m_Device->fnCreateRenderPass(dev, &rpci, nullptr, &m_RenderPass);

		// --- Framebuffer ---
		std::vector<VkImageView> fbViews;
		for (auto& v : m_ColorViews) fbViews.push_back(v);
		if (m_DepthView != VK_NULL_HANDLE) fbViews.push_back(m_DepthView);

		VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass      = m_RenderPass;
		fci.attachmentCount = static_cast<uint32_t>(fbViews.size());
		fci.pAttachments    = fbViews.data();
		fci.width           = w;
		fci.height          = h;
		fci.layers          = 1;

		m_Device->fnCreateFramebuffer(dev, &fci, nullptr, &m_Framebuffer);

		CANDY_CORE_INFO("VulkanFramebuffer: created {}x{} ({} color, {} depth)", w, h, colorCount, m_DepthView ? 1 : 0);
	}

	void VulkanFramebuffer::Bind()   {}
	void VulkanFramebuffer::Unbind() {}
	void VulkanFramebuffer::ClearAttachment(uint32_t, int) {}

	void VulkanFramebuffer::Resize(uint32_t w, uint32_t h)
	{
		if (w == 0 || h == 0) return;
		m_Desc.Width  = w;
		m_Desc.Height = h;
		if (!m_Desc.SwapChainTarget) Invalidate();
	}

	int VulkanFramebuffer::ReadPixel(uint32_t, int, int) { return -1; }
	uint32_t VulkanFramebuffer::GetColorAttachmentRendererID(uint32_t) const { return 0; }
	uint64_t VulkanFramebuffer::GetColorAttachmentGPUHandle(uint32_t) const { return 0; }

	VkImageView VulkanFramebuffer::GetColorView(uint32_t idx) const
	{
		return (idx < m_ColorViews.size()) ? m_ColorViews[idx] : VK_NULL_HANDLE;
	}

} // namespace Candy
