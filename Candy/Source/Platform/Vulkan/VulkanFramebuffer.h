#pragma once

#include "Runtime/Renderer/Framebuffer.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Candy {

	class VulkanDevice;

	// =========================================================================
	// VulkanFramebuffer — off-screen render target pending full implementation
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

	private:
		FramebufferSpecification m_Specification;
		VulkanDevice*            m_Device = nullptr;
	};

} // namespace Candy
