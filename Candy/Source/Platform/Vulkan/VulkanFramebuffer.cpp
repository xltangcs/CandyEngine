#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& spec, VulkanDevice* device)
		: m_Specification(spec), m_Device(device)
	{
		CANDY_CORE_WARN("VulkanFramebuffer: stub — full VkImage/VkFramebuffer/RenderPass pending");
	}

	VulkanFramebuffer::~VulkanFramebuffer() = default;

	void VulkanFramebuffer::Bind()   {}
	void VulkanFramebuffer::Unbind() {}

	void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Specification.Width  = width;
		m_Specification.Height = height;
	}

	int VulkanFramebuffer::ReadPixel(uint32_t, int, int) { return -1; }
	void VulkanFramebuffer::ClearAttachment(uint32_t, int) {}
	uint32_t VulkanFramebuffer::GetColorAttachmentRendererID(uint32_t) const { return 0; }
	uint64_t VulkanFramebuffer::GetColorAttachmentGPUHandle(uint32_t) const { return 0; }

} // namespace Candy
