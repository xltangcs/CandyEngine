#pragma once

#include "Runtime/RHI/RHICommandBuffer.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace Candy {

	class VulkanDevice;

	class VulkanCommandBuffer : public RHICommandBuffer
	{
	public:
		VulkanCommandBuffer(VulkanDevice* device, VkCommandPool pool, VkCommandBuffer cb);
		virtual ~VulkanCommandBuffer();

		void Begin() override;
		void End()   override;
		void BeginRenderPass(const RenderPassDesc& desc) override;
		void EndRenderPass() override;
		void SetPipeline(const Candy::Ref<RHIGraphicsPipeline>& pipeline) override;
		void SetViewport(float x, float y, float width, float height,
		                 float minDepth = 0.0f, float maxDepth = 1.0f) override;
		void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;
		void SetVertexBuffer(const Candy::Ref<RHIBuffer>& buffer, uint32_t slot = 0, uint64_t offset = 0) override;
		void SetIndexBuffer(const Candy::Ref<RHIBuffer>& buffer, IndexFormat format = IndexFormat::UInt32, uint64_t offset = 0) override;
		void SetConstantBuffer(uint32_t slot, uint32_t binding, const Candy::Ref<RHIBuffer>& buffer) override;
		void SetTexture(uint32_t slot, uint32_t binding, const Candy::Ref<RHITexture>& texture) override;
		void SetSampler(uint32_t slot, uint32_t binding, const Candy::Ref<RHISampler>& sampler) override;
		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
		          uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
		                 uint32_t firstIndex = 0, int32_t vertexOffset = 0,
		                 uint32_t firstInstance = 0) override;

		[[nodiscard]] VkCommandBuffer GetVkCommandBuffer() const { return m_CommandBuffer; }
		void SetRenderPassInfo(VkRenderPass rp, VkFramebuffer fb, VkExtent2D extent, const float* clearColor);

	private:
		VulkanDevice*    m_DevicePtr    = nullptr;
		VkCommandPool    m_Pool         = VK_NULL_HANDLE;
		VkCommandBuffer  m_CommandBuffer = VK_NULL_HANDLE;

		VkRenderPass     m_ActiveRenderPass  = VK_NULL_HANDLE;
		VkFramebuffer    m_ActiveFramebuffer = VK_NULL_HANDLE;
		VkExtent2D       m_ActiveExtent      = {};
		float            m_ClearColor[4]     = { 0,0,0,1 };
	};

} // namespace Candy
