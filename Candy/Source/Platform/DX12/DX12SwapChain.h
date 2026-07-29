#pragma once

#include "Runtime/RHI/RHISwapChain.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

namespace Candy {

	// =========================================================================
	// DX12SwapChain — Direct3D 12 swap chain
	//
	// Owns IDXGISwapChain3, back-buffer ID3D12Resources, and RTV descriptor
	// heap.  Created by DX12Device::CreateSwapChain().
	// =========================================================================
	class DX12SwapChain : public RHISwapChain
	{
	public:
		DX12SwapChain(ID3D12Device* device, IDXGIFactory6* factory,
		              ID3D12CommandQueue* queue, const SwapChainDesc& desc);
		virtual ~DX12SwapChain();

		const SwapChainDesc& GetDesc() const override;

		Candy::Ref<RHITexture> GetCurrentBackBuffer() override;

		void Resize(uint32_t width, uint32_t height) override;

		uint32_t GetWidth()  const override;
		uint32_t GetHeight() const override;

		// ---- DX12-specific accessors --------------------------------------

		[[nodiscard]] ID3D12Resource* GetCurrentBackBufferResource() const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const;
		[[nodiscard]] ID3D12DescriptorHeap*       GetRTVHeap() const { return m_RTVHeap.Get(); }
		[[nodiscard]] uint32_t                    GetRTVDescriptorSize() const { return m_RTVDescriptorSize; }
		[[nodiscard]] IDXGISwapChain3*            GetSwapChain() const { return m_SwapChain.Get(); }

		/// Call after Present to move to the next back buffer
		void AdvanceFrame();

	private:
		void CreateSwapChainResources();
		void ReleaseSwapChainResources();

		SwapChainDesc m_Desc;

		// Device references (non-owning)
		ID3D12Device*        m_Device = nullptr;
		IDXGIFactory6*       m_Factory = nullptr;
		ID3D12CommandQueue*  m_Queue   = nullptr;

		// DXGI swap chain
		Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;

		// RTV descriptor heap
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		uint32_t m_RTVDescriptorSize = 0;

		// Back buffers
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_BackBuffers;
		uint32_t m_CurrentBackBufferIndex = 0;
	};

} // namespace Candy
