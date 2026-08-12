#include "CandyPCH.h"
#include <Windows.h>

#include "Platform/D3D12/D3D12RendererAPI.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12CommandBuffer.h"
#include "Runtime/RHI/RHICommandBuffer.h"
#include "Runtime/RHI/RHISwapChain.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	void D3D12RendererAPI::Init(const PipelineStateDescription& defaultState)
	{
		CANDY_CORE_INFO("D3D12RendererAPI::Init — Blend={}, DepthTest={}, LineSmooth={}",
		                defaultState.Blend, defaultState.DepthTest, defaultState.LineSmooth);
		// D3D12 doesn't have global pipeline state; state is baked into PSOs.
		// DefaultState is logged for diagnostics, not applied globally.
	}

	void D3D12RendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		m_VpX = x; m_VpY = y; m_VpWidth = width; m_VpHeight = height;

		if (m_CmdBuffer)
		{
			m_CmdBuffer->SetViewport(static_cast<float>(x), static_cast<float>(y),
			                         static_cast<float>(width), static_cast<float>(height));
		}
	}

	void D3D12RendererAPI::SetClearColor(const glm::vec4& color)
	{
		m_ClearColor = color;
	}

	void D3D12RendererAPI::Clear()
	{
		// D3D12 clears happen inside BeginRenderPass, not as a standalone operation.
		// If we have a command buffer, set up a temporary render pass for the clear.
		if (m_CmdBuffer && m_SwapChain)
		{
			RenderPassDesc rpDesc;
			{
				RenderPassColorAttachment colorAttachment;
				colorAttachment.Format       = RHIFormat::B8G8R8A8Unorm;
				colorAttachment.LoadOp       = LoadOp::Clear;
				colorAttachment.ClearColor[0] = m_ClearColor.r;
				colorAttachment.ClearColor[1] = m_ClearColor.g;
				colorAttachment.ClearColor[2] = m_ClearColor.b;
				colorAttachment.ClearColor[3] = m_ClearColor.a;
				rpDesc.ColorAttachments.push_back(colorAttachment);
			}

			auto* d3d12cb = static_cast<D3D12CommandBuffer*>(m_CmdBuffer);
			d3d12cb->BeginRenderPass(rpDesc);
			d3d12cb->EndRenderPass();
		}
	}

	void D3D12RendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount)
	{
		(void)vertexArray; (void)topology; (void)indexCount;
		CANDY_CORE_WARN("D3D12RendererAPI::DrawIndexed — TODO: Renderer2D integration needed");
	}

	void D3D12RendererAPI::Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount)
	{
		(void)vertexArray; (void)topology; (void)elementCount;
		CANDY_CORE_WARN("D3D12RendererAPI::Draw — TODO: Renderer2D integration needed");
	}

	void D3D12RendererAPI::SetLineWidth(float width)
	{
		(void)width;
		// D3D12 does not support programmable line width; use geometry shader or ignore.
	}

	void D3D12RendererAPI::SetDefaultPipelineState(const PipelineStateDescription& state)
	{
		(void)state;
		// D3D12 pipeline state is baked into PSOs, not set globally.
	}

} // namespace Candy
