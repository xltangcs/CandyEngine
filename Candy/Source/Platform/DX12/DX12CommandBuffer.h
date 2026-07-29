#pragma once

#include "Runtime/RHI/RHICommandBuffer.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Candy {

	class DX12SwapChain;

	// =========================================================================
	// DX12CommandBuffer — wraps ID3D12GraphicsCommandList recording
	// =========================================================================
	class DX12CommandBuffer : public RHICommandBuffer
	{
	public:
		DX12CommandBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
		                  ID3D12CommandAllocator* allocator,
		                  ID3D12Device* device,
		                  ID3D12DescriptorHeap* cbvSrvUavHeap,
		                  ID3D12DescriptorHeap* samplerHeap);
		virtual ~DX12CommandBuffer();

		// ---- Lifetime ------------------------------------------------------

		void Begin() override;
		void End()   override;

		// ---- Render pass ---------------------------------------------------

		void BeginRenderPass(const RenderPassDesc& desc) override;
		void EndRenderPass() override;

		/// Set the swap chain as the current render target.
		void SetSwapChainRenderTarget(DX12SwapChain* swapChain);

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

		// ---- DX12-specific ------------------------------------------------

		[[nodiscard]] ID3D12GraphicsCommandList* GetNativeCommandList() const { return m_CommandList.Get(); }

	private:
		/// Allocate the next descriptor from the CBV/SRV/UAV heap.
		D3D12_CPU_DESCRIPTOR_HANDLE AllocateCBVSRVDescriptor();

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
		ID3D12CommandAllocator*                            m_Allocator  = nullptr;
		ID3D12Device*                                      m_Device     = nullptr;

		// Descriptor heaps (non-owning)
		ID3D12DescriptorHeap* m_CBVSRVUAVHeap = nullptr;
		ID3D12DescriptorHeap* m_SamplerHeap   = nullptr;
		uint32_t              m_CBVSRVDescriptorSize = 0;

		// Current render target (swap chain)
		DX12SwapChain* m_CurrentSwapChain = nullptr;

		// Simple linear descriptor allocator
		D3D12_CPU_DESCRIPTOR_HANDLE m_NextCBVSRVHandle = {};
	};

} // namespace Candy
