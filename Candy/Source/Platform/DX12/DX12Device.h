#pragma once

#include "Runtime/RHI/IR/IRDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>

namespace Candy {

	// =========================================================================
	// DX12Device — Direct3D 12 backend implementation
	//
	// Owns ID3D12Device, IDXGIFactory6, ID3D12CommandQueue, and a fence for
	// GPU-CPU synchronization.
	// =========================================================================
	class DX12Device : public IR::IRDevice
	{
	public:
		DX12Device();
		virtual ~DX12Device();

		// ---- Resource creation ----------------------------------------------

		Candy::Ref<RHIBuffer>   CreateBuffer(const BufferDesc& desc) override;
		Candy::Ref<RHITexture>  CreateTexture(const TextureDesc& desc) override;
		Candy::Ref<RHISampler>  CreateSampler(const SamplerDesc& desc) override;

		Candy::Ref<RHIShaderModule> CreateShaderModule(const void* spirvBytecode, uint32_t byteSize, const std::string& debugName = "") override;

		Candy::Ref<RHIGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const Candy::Ref<RHIShaderModule>& vs, const Candy::Ref<RHIShaderModule>& fs) override;

		Candy::Ref<RHISwapChain> CreateSwapChain(const SwapChainDesc& desc) override;

		// ---- Command submission --------------------------------------------

		RHICommandQueue& GetCommandQueue() override;

		// ---- Query ---------------------------------------------------------

		void WaitIdle() override;

		// ---- DX12-specific native accessors --------------------------------

		[[nodiscard]] ID3D12Device*        GetNativeDevice()  const { return m_NativeDevice.Get(); }
		[[nodiscard]] IDXGIFactory6*       GetNativeFactory() const { return m_Factory.Get(); }
		[[nodiscard]] ID3D12CommandQueue*  GetNativeQueue()   const;

		/// Signal the internal fence to a new value; returns the fence value.
		uint64_t SignalFence();

		/// CPU-wait until the fence reaches the given value.
		void WaitForFenceValue(uint64_t value);

	private:
		Microsoft::WRL::ComPtr<ID3D12Device>        m_NativeDevice;
		Microsoft::WRL::ComPtr<IDXGIFactory6>       m_Factory;
		Candy::Scope<RHICommandQueue>               m_CommandQueue;

		// Synchronization
		Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
		uint64_t                            m_FenceValue = 0;
		HANDLE                              m_FenceEvent  = nullptr;
	};

} // namespace Candy
