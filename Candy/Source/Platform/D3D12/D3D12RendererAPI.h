#pragma once

#include "Runtime/Renderer/RendererAPI.h"

namespace Candy {

	class D3D12Device;
	class RHICommandBuffer;
	class RHISwapChain;

	// =========================================================================
	// D3D12RendererAPI — implements RendererAPI for the Direct3D 12 backend
	//
	// Unlike OpenGL which has global state, D3D12 requires an explicit command
	// buffer for recording.  Call SetCommandBuffer() before each frame to
	// provide the current buffer.  Without a command buffer, Clear()/Draw*()
	// will be no-ops with a warning.
	// =========================================================================
	class D3D12RendererAPI : public RendererAPI
	{
	public:
		D3D12RendererAPI() = default;

		void Init(const PipelineStateDescription& defaultState) override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;

		void DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount = 0) override;
		void Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount) override;

		void SetLineWidth(float width) override;
		void SetDefaultPipelineState(const PipelineStateDescription& state) override;

		/// Must be called once to link this API to a D3D12 device.
		void SetDevice(D3D12Device* device) { m_Device = device; }

		/// Set the current command buffer for recording draw calls.
		void SetCommandBuffer(RHICommandBuffer* cmdBuffer) { m_CmdBuffer = cmdBuffer; }

		/// Set the current swap chain for target resolution.
		void SetSwapChain(RHISwapChain* swapChain) { m_SwapChain = swapChain; }

	private:
		D3D12Device*       m_Device     = nullptr;
		RHICommandBuffer* m_CmdBuffer  = nullptr;
		RHISwapChain*     m_SwapChain  = nullptr;

		glm::vec4         m_ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

		// Cached viewport for resize handling
		uint32_t m_VpX      = 0;
		uint32_t m_VpY      = 0;
		uint32_t m_VpWidth  = 0;
		uint32_t m_VpHeight = 0;
	};

} // namespace Candy
