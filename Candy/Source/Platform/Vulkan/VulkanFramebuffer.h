#pragma once

#include "Runtime/Renderer/Framebuffer.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Candy {

	class VulkanDevice;

	// =========================================================================
	// VulkanFramebuffer — VkImage + VkRenderPass + VkFramebuffer
	// =========================================================================
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const FramebufferSpecification& spec, VulkanDevice* device);
		virtual ~VulkanFramebuffer();

		void Bind() override;
		void Unbind() override;
		void Resize(uint32_t width, uint32_t height) override;
		int  ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		void ClearAttachment(uint32_t attachmentIndex, int value) override;
		uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override;
		uint64_t GetColorAttachmentGPUHandle(uint32_t index = 0) const override;
		const FramebufferSpecification& GetSpecification() const override { return m_Specification; }
		bool IsSwapChainTarget() const { return m_Specification.SwapChainTarget; }

		// Vulkan-specific accessors
		[[nodiscard]] VkRenderPass  GetRenderPass()  const { return m_RenderPass; }
		[[nodiscard]] VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
		[[nodiscard]] uint32_t      GetWidth()       const { return m_Specification.Width; }
		[[nodiscard]] uint32_t      GetHeight()      const { return m_Specification.Height; }
		[[nodiscard]] uint32_t      GetColorCount()  const { return static_cast<uint32_t>(m_ColorViews.size()); }
		[[nodiscard]] VkImageView   GetColorView(uint32_t idx) const;
		[[nodiscard]] bool          HasDepth() const { return m_DepthView != VK_NULL_HANDLE; }

	private:
		void Invalidate();
		VkFormat MapFormat(FramebufferTextureFormat fmt) const;

		FramebufferSpecification m_Specification;
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

		std::vector<FramebufferTextureSpecification> m_ColorSpecs;
		FramebufferTextureSpecification              m_DepthSpec = FramebufferTextureFormat::None;
	};

} // namespace Candy
