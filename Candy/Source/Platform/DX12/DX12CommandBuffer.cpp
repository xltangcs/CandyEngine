#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12CommandBuffer.h"
#include "Platform/DX12/DX12Buffer.h"
#include "Platform/DX12/DX12SwapChain.h"
#include "Platform/DX12/DX12PipelineState.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	DX12CommandBuffer::DX12CommandBuffer(ComPtr<ID3D12GraphicsCommandList> cmdList,
	                                     ID3D12CommandAllocator* allocator)
		: m_CommandList(std::move(cmdList)), m_Allocator(allocator)
	{
	}

	DX12CommandBuffer::~DX12CommandBuffer()
	{
	}

	// ---- Lifetime ------------------------------------------------------------

	void DX12CommandBuffer::Begin()
	{
		// Reset allocator and command list
		HRESULT hr = m_Allocator->Reset();
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12CommandBuffer::Begin: allocator Reset failed");
			return;
		}

		hr = m_CommandList->Reset(m_Allocator, nullptr);
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12CommandBuffer::Begin: command list Reset failed");
		}
	}

	void DX12CommandBuffer::End()
	{
		HRESULT hr = m_CommandList->Close();
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12CommandBuffer::End: Close failed");
		}
	}

	// ---- Render pass ---------------------------------------------------------

	void DX12CommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		// For now, we assume a single color attachment (the swap chain back buffer).
		// A full implementation would use multiple RTVs and a DSV.
		//
		// The RTV handle should be set externally (e.g., from DX12SwapChain) before
		// calling BeginRenderPass.  For now, this is a placeholder that would be
		// completed when the full render graph is wired up.
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::BeginRenderPass — RTV/DSV binding needs swap chain integration");
	}

	void DX12CommandBuffer::EndRenderPass()
	{
		// In DX12, there is no explicit EndRenderPass — render targets are
		// transitioned via resource barriers.
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::EndRenderPass — resource barrier transition needed");
	}

	// ---- Pipeline & state ----------------------------------------------------

	void DX12CommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		auto* dx12pso = dynamic_cast<DX12GraphicsPipeline*>(pipeline.get());
		if (dx12pso && dx12pso->GetNativePipelineState())
		{
			m_CommandList->SetPipelineState(dx12pso->GetNativePipelineState());
			m_CommandList->SetGraphicsRootSignature(dx12pso->GetRootSignature());

			// Set primitive topology
			D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			switch (dx12pso->GetDesc().Topology)
			{
			case PrimitiveTopology::Triangles:     topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
			case PrimitiveTopology::Lines:         topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
			case PrimitiveTopology::Points:        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
			case PrimitiveTopology::TriangleStrip: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
			case PrimitiveTopology::LineStrip:     topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
			default: break;
			}
			m_CommandList->IASetPrimitiveTopology(topology);
		}
		else
		{
			CANDY_CORE_WARN("DX12CommandBuffer::SetPipeline: pipeline not yet created or not a DX12 pipeline");
		}
	}

	void DX12CommandBuffer::SetViewport(float x, float y, float width, float height,
	                                    float minDepth, float maxDepth)
	{
		D3D12_VIEWPORT viewport = { x, y, width, height, minDepth, maxDepth };
		m_CommandList->RSSetViewports(1, &viewport);
	}

	void DX12CommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		D3D12_RECT scissor = {
			static_cast<LONG>(x),
			static_cast<LONG>(y),
			static_cast<LONG>(x + width),
			static_cast<LONG>(y + height)
		};
		m_CommandList->RSSetScissorRects(1, &scissor);
	}

	// ---- Resource binding ----------------------------------------------------

	void DX12CommandBuffer::SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot, uint64_t offset)
	{
		auto* dx12buffer = dynamic_cast<DX12Buffer*>(buffer.get());
		if (dx12buffer)
		{
			D3D12_VERTEX_BUFFER_VIEW vbv = {};
			vbv.BufferLocation = dx12buffer->GetGPUVirtualAddress() + offset;
			vbv.SizeInBytes    = static_cast<UINT>(dx12buffer->GetDesc().Size - offset);
			vbv.StrideInBytes  = 0; // Set manually if stride is known; TODO: get from pipeline desc

			m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
		}
		else
		{
			CANDY_CORE_WARN("DX12CommandBuffer::SetVertexBuffer: buffer is not a DX12Buffer");
		}
	}

	void DX12CommandBuffer::SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format, uint64_t offset)
	{
		auto* dx12buffer = dynamic_cast<DX12Buffer*>(buffer.get());
		if (dx12buffer)
		{
			D3D12_INDEX_BUFFER_VIEW ibv = {};
			ibv.BufferLocation = dx12buffer->GetGPUVirtualAddress() + offset;
			ibv.SizeInBytes    = static_cast<UINT>(dx12buffer->GetDesc().Size - offset);
			ibv.Format         = (format == IndexFormat::UInt16)
			                     ? DXGI_FORMAT_R16_UINT
			                     : DXGI_FORMAT_R32_UINT;

			m_CommandList->IASetIndexBuffer(&ibv);
		}
		else
		{
			CANDY_CORE_WARN("DX12CommandBuffer::SetIndexBuffer: buffer is not a DX12Buffer");
		}
	}

	void DX12CommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetConstantBuffer — root parameter binding needed");
	}

	void DX12CommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetTexture — descriptor heap binding needed");
	}

	void DX12CommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetSampler — sampler descriptor heap needed");
	}

	// ---- Draw calls ----------------------------------------------------------

	void DX12CommandBuffer::Draw(uint32_t vertexCount,
	                             uint32_t instanceCount,
	                             uint32_t firstVertex,
	                             uint32_t firstInstance)
	{
		m_CommandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void DX12CommandBuffer::DrawIndexed(uint32_t indexCount,
	                                    uint32_t instanceCount,
	                                    uint32_t firstIndex,
	                                    int32_t  vertexOffset,
	                                    uint32_t firstInstance)
	{
		m_CommandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

} // namespace Candy
