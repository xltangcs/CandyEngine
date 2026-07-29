#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanPipelineState.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice* device, VkCommandPool pool, VkCommandBuffer cb)
		: m_DevicePtr(device), m_Pool(pool), m_CommandBuffer(cb) {}

	VulkanCommandBuffer::~VulkanCommandBuffer() = default;

	void VulkanCommandBuffer::Begin()
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
	}

	void VulkanCommandBuffer::End()
	{
		vkEndCommandBuffer(m_CommandBuffer);
	}

	void VulkanCommandBuffer::SetRenderPassInfo(VkRenderPass rp, VkFramebuffer fb, VkExtent2D extent, const float* clearColor)
	{
		m_ActiveRenderPass  = rp;
		m_ActiveFramebuffer = fb;
		m_ActiveExtent      = extent;
		if (clearColor)
		{
			m_ClearColor[0] = clearColor[0]; m_ClearColor[1] = clearColor[1];
			m_ClearColor[2] = clearColor[2]; m_ClearColor[3] = clearColor[3];
		}
	}

	void VulkanCommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		if (!m_ActiveRenderPass)
		{
			CANDY_CORE_WARN("VulkanCommandBuffer::BeginRenderPass: no render pass set");
			return;
		}

		const float* cc = desc.ColorAttachments.empty() ? nullptr : desc.ColorAttachments[0].ClearColor;
		if (cc)
		{
			m_ClearColor[0] = cc[0]; m_ClearColor[1] = cc[1];
			m_ClearColor[2] = cc[2]; m_ClearColor[3] = cc[3];
		}

		VkClearValue clearValue;
		clearValue.color = { { m_ClearColor[0], m_ClearColor[1], m_ClearColor[2], m_ClearColor[3] } };

		VkRenderPassBeginInfo rpInfo = {};
		rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpInfo.renderPass        = m_ActiveRenderPass;
		rpInfo.framebuffer       = m_ActiveFramebuffer;
		rpInfo.renderArea.offset = { 0, 0 };
		rpInfo.renderArea.extent = m_ActiveExtent;
		rpInfo.clearValueCount   = 1;
		rpInfo.pClearValues      = &clearValue;

		vkCmdBeginRenderPass(m_CommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanCommandBuffer::EndRenderPass()
	{
		vkCmdEndRenderPass(m_CommandBuffer);
	}

	void VulkanCommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		auto* vkp = dynamic_cast<VulkanGraphicsPipeline*>(pipeline.get());
		if (vkp && vkp->GetVkPipeline())
			vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkp->GetVkPipeline());
	}

	void VulkanCommandBuffer::SetViewport(float x, float y, float w, float h, float minD, float maxD)
	{
		VkViewport vp = { x, y, w, h, minD, maxD };
		vkCmdSetViewport(m_CommandBuffer, 0, 1, &vp);
	}

	void VulkanCommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
	{
		VkRect2D sc = { { x, y }, { w, h } };
		vkCmdSetScissor(m_CommandBuffer, 0, 1, &sc);
	}

	void VulkanCommandBuffer::SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot, uint64_t offset)
	{
		auto* vkb = dynamic_cast<VulkanBuffer*>(buffer.get());
		if (vkb)
		{
			VkBuffer buf = vkb->GetVkBuffer();
			VkDeviceSize off = offset;
			vkCmdBindVertexBuffers(m_CommandBuffer, slot, 1, &buf, &off);
		}
	}

	void VulkanCommandBuffer::SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format, uint64_t offset)
	{
		auto* vkb = dynamic_cast<VulkanBuffer*>(buffer.get());
		if (vkb)
		{
			vkCmdBindIndexBuffer(m_CommandBuffer, vkb->GetVkBuffer(), offset,
				(format == IndexFormat::UInt16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
		}
	}

	void VulkanCommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>&) { CANDY_CORE_WARN("TODO: Vulkan SetConstantBuffer"); }
	void VulkanCommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>&)     { CANDY_CORE_WARN("TODO: Vulkan SetTexture"); }
	void VulkanCommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>&)     { CANDY_CORE_WARN("TODO: Vulkan SetSampler"); }

	void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
	                               uint32_t firstVertex, uint32_t firstInstance)
	{
		vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void VulkanCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
	                                      uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

} // namespace Candy
