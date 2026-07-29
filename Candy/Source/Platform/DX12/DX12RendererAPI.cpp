#include "CandyPCH.h"
#include <Windows.h>

#include "Platform/DX12/DX12RendererAPI.h"
#include "Platform/DX12/DX12Device.h"
#include "Platform/DX12/DX12CommandBuffer.h"
#include "Runtime/RHI/RHICommandBuffer.h"
#include "Runtime/RHI/RHISwapChain.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	void DX12RendererAPI::Init(const PipelineStateDescription& defaultState)
	{
		CANDY_CORE_INFO("DX12RendererAPI::Init — Blend={}, DepthTest={}, LineSmooth={}",
		                defaultState.Blend, defaultState.DepthTest, defaultState.LineSmooth);
		// DX12 doesn't have global pipeline state; state is baked into PSOs.
		// DefaultState is logged for diagnostics, not applied globally.
	}

	void DX12RendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		m_VpX = x; m_VpY = y; m_VpWidth = width; m_VpHeight = height;

		if (m_CmdBuffer)
		{
			m_CmdBuffer->SetViewport(static_cast<float>(x), static_cast<float>(y),
			                         static_cast<float>(width), static_cast<float>(height));
		}
	}

	void DX12RendererAPI::SetClearColor(const glm::vec4& color)
	{
		m_ClearColor = color;
	}

	void DX12RendererAPI::Clear()
	{
		// DX12 clears happen inside BeginRenderPass, not as a standalone operation.
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

			auto* dx12cb = static_cast<DX12CommandBuffer*>(m_CmdBuffer);
			dx12cb->BeginRenderPass(rpDesc);
			dx12cb->EndRenderPass();
		}
	}

	void DX12RendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount)
	{
		(void)vertexArray; (void)topology; (void)indexCount;
		CANDY_CORE_WARN("DX12RendererAPI::DrawIndexed — TODO: Renderer2D integration needed");
	}

	void DX12RendererAPI::Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount)
	{
		(void)vertexArray; (void)topology; (void)elementCount;
		CANDY_CORE_WARN("DX12RendererAPI::Draw — TODO: Renderer2D integration needed");
	}

	void DX12RendererAPI::SetLineWidth(float width)
	{
		(void)width;
		// DX12 does not support programmable line width; use geometry shader or ignore.
	}

	void DX12RendererAPI::SetDefaultPipelineState(const PipelineStateDescription& state)
	{
		(void)state;
		// DX12 pipeline state is baked into PSOs, not set globally.
	}

} // namespace Candy
