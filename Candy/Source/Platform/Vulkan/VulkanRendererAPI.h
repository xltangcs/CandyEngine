#pragma once

#include "Runtime/Renderer/RendererAPI.h"
#include <glm/glm.hpp>

namespace Candy {

	class VulkanDevice;
	class RHICommandBuffer;
	class RHISwapChain;
	class VulkanSwapChain;

	// =========================================================================
	// VulkanRendererAPI — delegates viewport/clear/draw to RHI command buffer
	// =========================================================================
	class VulkanRendererAPI : public RendererAPI
	{
	public:
		VulkanRendererAPI() = default;

		void Init(const PipelineStateDescription& defaultState) override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;
		void DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount = 0) override;
		void Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount) override;
		void SetLineWidth(float width) override;
		void SetDefaultPipelineState(const PipelineStateDescription& state) override;

		void SetDevice(VulkanDevice* device)       { m_Device = device; }
		void SetCommandBuffer(RHICommandBuffer* cb) { m_CmdBuffer = cb; }
		void SetSwapChain(RHISwapChain* sc)        { m_SwapChain = sc; }

	private:
		VulkanDevice*      m_Device      = nullptr;
		RHICommandBuffer*  m_CmdBuffer   = nullptr;
		RHISwapChain*      m_SwapChain   = nullptr;
		glm::vec4          m_ClearColor   = { 0.1f, 0.1f, 0.1f, 1.0f };
		uint32_t m_VpX = 0, m_VpY = 0, m_VpWidth = 0, m_VpHeight = 0;
	};

} // namespace Candy
