#pragma once

#include "Runtime/RHI/IR/IRDevice.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Candy {

	// =========================================================================
	// DX12Device — Direct3D 12 backend implementation
	//
	// Inherits from IR::IRDevice.  Uses ComPtr for automatic COM lifetime
	// management.
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

		[[nodiscard]] ID3D12Device* GetNativeDevice() const { return m_NativeDevice.Get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_NativeDevice;
		Candy::Scope<RHICommandQueue>        m_CommandQueue;
	};

} // namespace Candy
