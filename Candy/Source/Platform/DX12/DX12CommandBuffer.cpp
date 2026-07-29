#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12CommandBuffer.h"
#include "Platform/DX12/DX12Buffer.h"
#include "Platform/DX12/DX12SwapChain.h"
#include "Platform/DX12/DX12PipelineState.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	DX12CommandBuffer::DX12CommandBuffer(ComPtr<ID3D12GraphicsCommandList> cmdList,
	                                     ID3D12CommandAllocator* allocator,
	                                     ID3D12Device* device,
	                                     ID3D12DescriptorHeap* cbvSrvUavHeap,
	                                     ID3D12DescriptorHeap* samplerHeap)
		: m_CommandList(std::move(cmdList))
		, m_Allocator(allocator)
		, m_Device(device)
		, m_CBVSRVUAVHeap(cbvSrvUavHeap)
		, m_SamplerHeap(samplerHeap)
	{
		if (cbvSrvUavHeap)
		{
			m_NextCBVSRVHandle = m_CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart();
			m_CBVSRVDescriptorSize = device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}

	DX12CommandBuffer::~DX12CommandBuffer() = default;

	// ---- Descriptor heap allocation ------------------------------------------

	D3D12_CPU_DESCRIPTOR_HANDLE DX12CommandBuffer::AllocateCBVSRVDescriptor()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_NextCBVSRVHandle;
		m_NextCBVSRVHandle.ptr += m_CBVSRVDescriptorSize;
		return handle;
	}

	// ---- Lifetime ------------------------------------------------------------

	void DX12CommandBuffer::Begin()
	{
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
			return;
		}

		// Bind descriptor heaps
		ID3D12DescriptorHeap* heaps[] = { m_CBVSRVUAVHeap, m_SamplerHeap };
		m_CommandList->SetDescriptorHeaps(2, heaps);

		// Reset linear descriptor allocator
		if (m_CBVSRVUAVHeap)
			m_NextCBVSRVHandle = m_CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void DX12CommandBuffer::End()
	{
		HRESULT hr = m_CommandList->Close();
		if (FAILED(hr))
			CANDY_CORE_ERROR("DX12CommandBuffer::End: Close failed");
	}

	// ---- Render target -------------------------------------------------------

	void DX12CommandBuffer::SetSwapChainRenderTarget(DX12SwapChain* swapChain)
	{
		m_CurrentSwapChain = swapChain;
	}

	// ---- Render pass ---------------------------------------------------------

	void DX12CommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		if (!m_CurrentSwapChain)
		{
			CANDY_CORE_WARN("DX12CommandBuffer::BeginRenderPass: no swap chain set as render target");
			return;
		}

		ID3D12Resource* backBuffer = m_CurrentSwapChain->GetCurrentBackBufferResource();
		if (!backBuffer)
		{
			CANDY_CORE_ERROR("DX12CommandBuffer::BeginRenderPass: null back buffer");
			return;
		}

		// Transition: PRESENT → RENDER_TARGET
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = backBuffer;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		m_CommandList->ResourceBarrier(1, &barrier);

		// Bind RTV and clear
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_CurrentSwapChain->GetCurrentRTVHandle();
		const float* clearColor = desc.ColorAttachments.empty()
			? nullptr : desc.ColorAttachments[0].ClearColor;

		FLOAT rgba[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		if (clearColor)
		{
			rgba[0] = clearColor[0]; rgba[1] = clearColor[1];
			rgba[2] = clearColor[2]; rgba[3] = clearColor[3];
		}
		m_CommandList->ClearRenderTargetView(rtvHandle, rgba, 0, nullptr);
		m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	}

	void DX12CommandBuffer::EndRenderPass()
	{
		if (m_CurrentSwapChain)
		{
			ID3D12Resource* backBuffer = m_CurrentSwapChain->GetCurrentBackBufferResource();
			if (backBuffer)
			{
				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.pResource   = backBuffer;
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				m_CommandList->ResourceBarrier(1, &barrier);
			}
		}
	}

	// ---- Pipeline & state ----------------------------------------------------

	void DX12CommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		auto* dx12pso = dynamic_cast<DX12GraphicsPipeline*>(pipeline.get());
		if (dx12pso && dx12pso->GetNativePipelineState())
		{
			m_CommandList->SetPipelineState(dx12pso->GetNativePipelineState());
			m_CommandList->SetGraphicsRootSignature(dx12pso->GetRootSignature());

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
	}

	void DX12CommandBuffer::SetViewport(float x, float y, float width, float height,
	                                    float minDepth, float maxDepth)
	{
		D3D12_VIEWPORT vp = { x, y, width, height, minDepth, maxDepth };
		m_CommandList->RSSetViewports(1, &vp);
	}

	void DX12CommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		D3D12_RECT sc = { static_cast<LONG>(x), static_cast<LONG>(y),
		                  static_cast<LONG>(x + width), static_cast<LONG>(y + height) };
		m_CommandList->RSSetScissorRects(1, &sc);
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
			vbv.StrideInBytes  = 7 * sizeof(float); // pos(3) + color(4)

			m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
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
			                     ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

			m_CommandList->IASetIndexBuffer(&ibv);
		}
	}

	void DX12CommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer)
	{
		auto* dx12buffer = dynamic_cast<DX12Buffer*>(buffer.get());
		if (!dx12buffer || !m_Device)
			return;

		// Create CBV descriptor
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = dx12buffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes    = static_cast<UINT>(
			(dx12buffer->GetDesc().Size + 255) & ~255ull);

		D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = AllocateCBVSRVDescriptor();
		m_Device->CreateConstantBufferView(&cbvDesc, cbvHandle);

		// Bind to root parameter 0 (CBV)
		m_CommandList->SetGraphicsRootConstantBufferView(0, dx12buffer->GetGPUVirtualAddress());
	}

	void DX12CommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetTexture");
	}

	void DX12CommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetSampler");
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
