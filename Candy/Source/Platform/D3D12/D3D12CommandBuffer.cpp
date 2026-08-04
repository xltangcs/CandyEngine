#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>

#include "Platform/D3D12/D3D12CommandBuffer.h"
#include "Platform/D3D12/D3D12Buffer.h"
#include "Platform/D3D12/D3D12SwapChain.h"
#include "Platform/D3D12/D3D12Framebuffer.h"
#include "Platform/D3D12/D3D12PipelineState.h"
#include "Platform/D3D12/D3D12Texture.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	D3D12CommandBuffer::D3D12CommandBuffer(ComPtr<ID3D12GraphicsCommandList> cmdList,
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

	D3D12CommandBuffer::~D3D12CommandBuffer() = default;

	// ---- Descriptor heap allocation ------------------------------------------

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12CommandBuffer::AllocateCBVSRVDescriptor()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_NextCBVSRVHandle;
		m_NextCBVSRVHandle.ptr += m_CBVSRVDescriptorSize;
		return handle;
	}

	// ---- Lifetime ------------------------------------------------------------

	void D3D12CommandBuffer::Begin()
	{
		HRESULT hr = m_Allocator->Reset();
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12CommandBuffer::Begin: allocator Reset failed");
			return;
		}

		hr = m_CommandList->Reset(m_Allocator, nullptr);
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12CommandBuffer::Begin: command list Reset failed");
			return;
		}

		// Bind descriptor heaps
		ID3D12DescriptorHeap* heaps[] = { m_CBVSRVUAVHeap, m_SamplerHeap };
		m_CommandList->SetDescriptorHeaps(2, heaps);

		// Reset linear descriptor allocator
		if (m_CBVSRVUAVHeap)
			m_NextCBVSRVHandle = m_CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void D3D12CommandBuffer::End()
	{
		HRESULT hr = m_CommandList->Close();
		if (FAILED(hr))
			CANDY_CORE_ERROR("D3D12CommandBuffer::End: Close failed");
	}

	// ---- Render target -------------------------------------------------------

	void D3D12CommandBuffer::SetSwapChainRenderTarget(D3D12SwapChain* swapChain)
	{
		m_CurrentSwapChain   = swapChain;
		m_CurrentFramebuffer = nullptr;
	}

	void D3D12CommandBuffer::SetFramebufferRenderTarget(const Ref<RHIFramebuffer>& framebuffer)
	{
		m_CurrentFramebuffer = dynamic_cast<D3D12Framebuffer*>(framebuffer.get());
		m_CurrentSwapChain   = nullptr;
	}

	// ---- Render pass ---------------------------------------------------------

	void D3D12CommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		// ---- Framebuffer target paths ----
		if (m_CurrentFramebuffer)
		{
			uint32_t colorCount = m_CurrentFramebuffer->GetColorAttachmentCount();
			CANDY_CORE_ASSERT(colorCount <= 4);
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[4] = {};
			for (uint32_t i = 0; i < colorCount; ++i)
				rtvHandles[i] = m_CurrentFramebuffer->GetRTVHandle(i);

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
			bool hasDepth = m_CurrentFramebuffer->HasDepthAttachment();
			if (hasDepth)
				dsvHandle = m_CurrentFramebuffer->GetDSVHandle();

			// Clear color attachments
			for (uint32_t i = 0; i < colorCount && i < desc.ColorAttachments.size(); ++i)
			{
				const float* cc = desc.ColorAttachments[i].ClearColor;
				FLOAT rgba[4] = { cc ? cc[0] : 0.0f, cc ? cc[1] : 0.0f,
				                  cc ? cc[2] : 0.0f, cc ? cc[3] : 1.0f };
				m_CommandList->ClearRenderTargetView(rtvHandles[i], rgba, 0, nullptr);
			}

			// Clear depth
			if (hasDepth)
			{
				m_CommandList->ClearDepthStencilView(dsvHandle,
					D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
					1.0f, 0, 0, nullptr);
			}

			m_CommandList->OMSetRenderTargets(colorCount, rtvHandles, FALSE,
				hasDepth ? &dsvHandle : nullptr);
			return;
		}

		// ---- Swap chain target paths -----
		if (!m_CurrentSwapChain)
		{
			CANDY_CORE_WARN("D3D12CommandBuffer::BeginRenderPass: no render target set");
			return;
		}

		ID3D12Resource* backBuffer = m_CurrentSwapChain->GetCurrentBackBufferResource();
		if (!backBuffer)
		{
			CANDY_CORE_ERROR("D3D12CommandBuffer::BeginRenderPass: null back buffer");
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

	void D3D12CommandBuffer::EndRenderPass()
	{
		if (m_CurrentFramebuffer)
		{
			// Framebuffer resources stay in RENDER_TARGET state.
			// Transition is handled by ReadPixel when needed.
			return;
		}

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

	void D3D12CommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		auto* d3d12pso = dynamic_cast<D3D12GraphicsPipeline*>(pipeline.get());
		if (d3d12pso && d3d12pso->GetNativePipelineState())
		{
			m_CommandList->SetPipelineState(d3d12pso->GetNativePipelineState());
			m_CommandList->SetGraphicsRootSignature(d3d12pso->GetRootSignature());

			D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			switch (d3d12pso->GetDesc().Topology)
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

	void D3D12CommandBuffer::SetViewport(float x, float y, float width, float height,
	                                    float minDepth, float maxDepth)
	{
		D3D12_VIEWPORT vp = { x, y, width, height, minDepth, maxDepth };
		m_CommandList->RSSetViewports(1, &vp);
	}

	void D3D12CommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		D3D12_RECT sc = { static_cast<LONG>(x), static_cast<LONG>(y),
		                  static_cast<LONG>(x + width), static_cast<LONG>(y + height) };
		m_CommandList->RSSetScissorRects(1, &sc);
	}

	// ---- Resource binding ----------------------------------------------------

	void D3D12CommandBuffer::SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot, uint64_t offset)
	{
		auto* d3d12buffer = dynamic_cast<D3D12Buffer*>(buffer.get());
		if (d3d12buffer)
		{
			const uint32_t stride = d3d12buffer->GetStride();
			if (stride == 0)
			{
				CANDY_CORE_WARN("D3D12CommandBuffer::SetVertexBuffer: buffer '{}' has zero stride; bind ignored",
				                 d3d12buffer->GetDesc().DebugName);
				return;
			}

			D3D12_VERTEX_BUFFER_VIEW vbv = {};
			vbv.BufferLocation = d3d12buffer->GetGPUVirtualAddress() + offset;
			vbv.SizeInBytes    = static_cast<UINT>(d3d12buffer->GetDesc().Size - offset);
			vbv.StrideInBytes  = stride;

			m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
		}
	}

	void D3D12CommandBuffer::SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format, uint64_t offset)
	{
		auto* d3d12buffer = dynamic_cast<D3D12Buffer*>(buffer.get());
		if (d3d12buffer)
		{
			D3D12_INDEX_BUFFER_VIEW ibv = {};
			ibv.BufferLocation = d3d12buffer->GetGPUVirtualAddress() + offset;
			ibv.SizeInBytes    = static_cast<UINT>(d3d12buffer->GetDesc().Size - offset);
			ibv.Format         = (format == IndexFormat::UInt16)
			                     ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

			m_CommandList->IASetIndexBuffer(&ibv);
		}
	}

	void D3D12CommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer)
	{
		auto* d3d12buffer = dynamic_cast<D3D12Buffer*>(buffer.get());
		if (!d3d12buffer || !m_CommandList)
			return;

		// Root descriptor model: bind the buffer's GPU virtual address
		// directly to a root CBV parameter.  No descriptor heap allocation
		// is required and the binding persists on this slot until the next
		// call to SetConstantBuffer with the same slot or End().
		(void)binding; // D3D12 root CBV does not use a descriptor table index
		m_CommandList->SetGraphicsRootConstantBufferView(slot, d3d12buffer->GetGPUVirtualAddress());
	}

	void D3D12CommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture)
	{
		auto* d3d12tex = dynamic_cast<D3D12Texture*>(texture.get());
		if (!d3d12tex || !d3d12tex->GetResource())
		{
			CANDY_CORE_WARN("D3D12CommandBuffer::SetTexture: not a D3D12Texture or resource is null");
			return;
		}
		if (!m_CBVSRVUAVHeap || !m_CommandList)
			return;

		// Write an SRV for this texture at offset `binding` from the heap
		// start.  Multiple textures in the same batch occupy consecutive
		// binding slots [0, N); the descriptor table bound to root parameter
		// `slot` covers them all in one SetGraphicsRootDescriptorTable call.
		d3d12tex->CreateSRV(m_CBVSRVUAVHeap, binding, m_CBVSRVDescriptorSize);

		// Bind the table starting at the heap base to root parameter `slot`.
		// Re-binding the whole table is cheap; it remains valid until End().
		D3D12_GPU_DESCRIPTOR_HANDLE gpuBase = m_CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart();
		m_CommandList->SetGraphicsRootDescriptorTable(slot, gpuBase);
	}

	void D3D12CommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler)
	{
		// Renderer2D's textured root signature (see
		// D3D12Device::CreateTexturedRootSignature) bakes a single linear
		// static sampler at s0 with linear/min-mag-mip filter.  Non-static
		// sampler heap binding is not yet wired through the RHI abstraction.
		(void)sampler; (void)slot; (void)binding;
		CANDY_CORE_TRACE("D3D12CommandBuffer::SetSampler({}, {}) — relying on static sampler", slot, binding);
	}

	// ---- Draw calls ----------------------------------------------------------

	void D3D12CommandBuffer::Draw(uint32_t vertexCount,
	                             uint32_t instanceCount,
	                             uint32_t firstVertex,
	                             uint32_t firstInstance)
	{
		m_CommandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void D3D12CommandBuffer::DrawIndexed(uint32_t indexCount,
	                                    uint32_t instanceCount,
	                                    uint32_t firstIndex,
	                                    int32_t  vertexOffset,
	                                    uint32_t firstInstance)
	{
		m_CommandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

} // namespace Candy
