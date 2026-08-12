#pragma once

#include "Runtime/Renderer/Framebuffer.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Candy {

	class VulkanDevice;

	// =========================================================================
	// [EXPERIMENTAL — FROZEN] Vulkan backend is parked; see AGENTS.md.
	// VulkanFramebuffer — VkImage + VkRenderPass + VkFramebuffer
	// =========================================================================
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const FramebufferDesc& desc, VulkanDevice* device);
		virtual ~VulkanFramebuffer();

		void Bind() override;
		void Unbind() override;
		void Resize(uint32_t width, uint32_t height) override;
		int  ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		void ClearAttachment(uint32_t attachmentIndex, int value) override;
		uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override;
		uint64_t GetColorAttachmentGPUHandle(uint32_t index = 0) const override;
		bool IsSwapChainTarget() const { return m_Desc.SwapChainTarget; }

		// ---- RHIFramebuffer (via Framebuffer) ---------------------------
		const FramebufferDesc& GetDesc()        const override { return m_Desc; }
		uint32_t GetWidth()                     const override { return m_Desc.Width; }
		uint32_t GetHeight()                    const override { return m_Desc.Height; }
		uint32_t GetColorAttachmentCount()      const override { return static_cast<uint32_t>(m_ColorViews.size()); }
		bool     HasDepthStencil()              const override { return m_Desc.HasDepthStencil; }

		// Vulkan-specific accessors
		[[nodiscard]] VkRenderPass  GetRenderPass()  const { return m_RenderPass; }
		[[nodiscard]] VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
		[[nodiscard]] uint32_t      GetColorCount()  const { return static_cast<uint32_t>(m_ColorViews.size()); }
		[[nodiscard]] VkImageView   GetColorView(uint32_t idx) const;
		[[nodiscard]] bool          HasDepth() const { return m_DepthView != VK_NULL_HANDLE; }

	private:
		void Invalidate();
		VkFormat MapFormat(RHIFormat fmt) const;

		FramebufferDesc          m_Desc;
		VulkanDevice*            m_Device = nullptr;

		// Color attachments
		std::vector<VkImage>        m_ColorImages;
		std::vector<VkDeviceMemory> m_ColorMemories;
		std::vector<VkImageView>    m_ColorViews;

		// Depth attachment
		VkImage        m_DepthImage   = VK_NULL_HANDLE;
		VkDeviceMemory m_DepthMemory  = VK_NULL_HANDLE;
		VkImageView    m_DepthView    = VK_NULL_HANDLE;

		VkRenderPass  m_RenderPass  = VK_NULL_HANDLE;
		VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
	};

} // namespace Candy
