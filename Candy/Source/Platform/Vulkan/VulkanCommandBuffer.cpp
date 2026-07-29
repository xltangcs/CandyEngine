#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanCommandBuffer::VulkanCommandBuffer()
	{
	}

	VulkanCommandBuffer::~VulkanCommandBuffer()
	{
	}

	// ---- Lifetime ------------------------------------------------------------

	void VulkanCommandBuffer::Begin()
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::Begin — not yet implemented");
	}

	void VulkanCommandBuffer::End()
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::End — not yet implemented");
	}

	// ---- Render pass ---------------------------------------------------------

	void VulkanCommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::BeginRenderPass — not yet implemented");
	}

	void VulkanCommandBuffer::EndRenderPass()
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::EndRenderPass — not yet implemented");
	}

	// ---- Pipeline & state ----------------------------------------------------

	void VulkanCommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetPipeline — not yet implemented");
	}

	void VulkanCommandBuffer::SetViewport(float x, float y, float width, float height,
	                                      float minDepth, float maxDepth)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetViewport — not yet implemented");
	}

	void VulkanCommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetScissor — not yet implemented");
	}

	// ---- Resource binding ----------------------------------------------------

	void VulkanCommandBuffer::SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot, uint64_t offset)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetVertexBuffer — not yet implemented");
	}

	void VulkanCommandBuffer::SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format, uint64_t offset)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetIndexBuffer — not yet implemented");
	}

	void VulkanCommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetConstantBuffer — not yet implemented");
	}

	void VulkanCommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetTexture — not yet implemented");
	}

	void VulkanCommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::SetSampler — not yet implemented");
	}

	// ---- Draw calls ----------------------------------------------------------

	void VulkanCommandBuffer::Draw(uint32_t vertexCount,
	                               uint32_t instanceCount,
	                               uint32_t firstVertex,
	                               uint32_t firstInstance)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::Draw — not yet implemented");
	}

	void VulkanCommandBuffer::DrawIndexed(uint32_t indexCount,
	                                      uint32_t instanceCount,
	                                      uint32_t firstIndex,
	                                      int32_t  vertexOffset,
	                                      uint32_t firstInstance)
	{
		CANDY_CORE_WARN("TODO: VulkanCommandBuffer::DrawIndexed — not yet implemented");
	}

} // namespace Candy
