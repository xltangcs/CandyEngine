#pragma once

#include "Runtime/RHI/RHISwapChain.h"

namespace Candy {

	// =========================================================================
	// DX12SwapChain — Direct3D 12 swap chain skeleton
	//
	// Creates IDXGISwapChain3 + RTV descriptor heap back buffers once the
	// DX12 Agility SDK is integrated.
	// =========================================================================
	class DX12SwapChain : public RHISwapChain
	{
	public:
		DX12SwapChain(const SwapChainDesc& desc);
		virtual ~DX12SwapChain();

		const SwapChainDesc& GetDesc() const override;

		Candy::Ref<RHITexture> GetCurrentBackBuffer() override;

		void Resize(uint32_t width, uint32_t height) override;

		uint32_t GetWidth()  const override;
		uint32_t GetHeight() const override;

	private:
		SwapChainDesc m_Desc;
	};

} // namespace Candy
