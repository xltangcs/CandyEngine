#pragma once

#include "Runtime/RHI/RHICommandBuffer.h"

namespace Candy {

	// =========================================================================
	// VulkanCommandBuffer — Vulkan command buffer skeleton
	//
	// Wraps VkCommandBuffer recording once the Vulkan SDK is integrated.
	// =========================================================================
	class VulkanCommandBuffer : public RHICommandBuffer
	{
	public:
		VulkanCommandBuffer();
		virtual ~VulkanCommandBuffer();

		// ---- Lifetime ------------------------------------------------------

		void Begin() override;
		void End()   override;

		// ---- Render pass ---------------------------------------------------

		void BeginRenderPass(const RenderPassDesc& desc) override;
		void EndRenderPass() override;

		// ---- Pipeline & state ----------------------------------------------

		void SetPipeline(const Candy::Ref<RHIGraphicsPipeline>& pipeline) override;

		void SetViewport(float x, float y, float width, float height,
		                 float minDepth = 0.0f, float maxDepth = 1.0f) override;

		void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

		// ---- Resource binding ----------------------------------------------

		void SetVertexBuffer(const Candy::Ref<RHIBuffer>& buffer, uint32_t slot = 0, uint64_t offset = 0) override;
		void SetIndexBuffer(const Candy::Ref<RHIBuffer>& buffer, IndexFormat format = IndexFormat::UInt32, uint64_t offset = 0) override;

		void SetConstantBuffer(uint32_t slot, uint32_t binding, const Candy::Ref<RHIBuffer>& buffer) override;
		void SetTexture(uint32_t slot, uint32_t binding, const Candy::Ref<RHITexture>& texture) override;
		void SetSampler(uint32_t slot, uint32_t binding, const Candy::Ref<RHISampler>& sampler) override;

		// ---- Draw calls ----------------------------------------------------

		void Draw(uint32_t vertexCount,
		          uint32_t instanceCount = 1,
		          uint32_t firstVertex   = 0,
		          uint32_t firstInstance = 0) override;

		void DrawIndexed(uint32_t indexCount,
		                 uint32_t instanceCount = 1,
		                 uint32_t firstIndex    = 0,
		                 int32_t  vertexOffset  = 0,
		                 uint32_t firstInstance = 0) override;
	};

} // namespace Candy
