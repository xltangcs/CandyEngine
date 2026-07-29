#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Platform/DX12/DX12SwapChain.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	DX12SwapChain::DX12SwapChain(const SwapChainDesc& desc)
		: m_Desc(desc)
	{
		CANDY_CORE_INFO("DX12SwapChain: created {}x{} (buffer count: {})",
		                desc.Width, desc.Height, desc.BufferCount);
	}

	DX12SwapChain::~DX12SwapChain()
	{
		CANDY_CORE_INFO("DX12SwapChain: destroyed");
	}

	const SwapChainDesc& DX12SwapChain::GetDesc() const
	{
		return m_Desc;
	}

	Ref<RHITexture> DX12SwapChain::GetCurrentBackBuffer()
	{
		CANDY_CORE_WARN("TODO: DX12SwapChain::GetCurrentBackBuffer — not yet implemented");
		return nullptr;
	}

	void DX12SwapChain::Resize(uint32_t width, uint32_t height)
	{
		m_Desc.Width  = width;
		m_Desc.Height = height;
		CANDY_CORE_INFO("DX12SwapChain::Resize {}x{}", width, height);
		CANDY_CORE_WARN("TODO: DX12SwapChain::Resize — actual buffer resize not yet implemented");
	}

	uint32_t DX12SwapChain::GetWidth() const
	{
		return m_Desc.Width;
	}

	uint32_t DX12SwapChain::GetHeight() const
	{
		return m_Desc.Height;
	}

} // namespace Candy
