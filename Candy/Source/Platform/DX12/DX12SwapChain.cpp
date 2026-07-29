#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Platform/DX12/DX12SwapChain.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	DX12SwapChain::DX12SwapChain(ID3D12Device* device, IDXGIFactory6* factory,
	                             ID3D12CommandQueue* queue, const SwapChainDesc& desc)
		: m_Desc(desc), m_Device(device), m_Factory(factory), m_Queue(queue)
	{
		CANDY_CORE_INFO("DX12SwapChain: creating {}x{} (buffer count: {}, vsync: {})",
		                desc.Width, desc.Height, desc.BufferCount, desc.VSync);

		// Determine swap chain format
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (desc.Format == RHIFormat::B8G8R8A8Unorm)
			format = DXGI_FORMAT_B8G8R8A8_UNORM;
		else if (desc.Format == RHIFormat::B8G8R8A8Srgb)
			format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

		// Create the DXGI swap chain
		HWND hwnd = static_cast<HWND>(desc.Window.Native);
		if (!hwnd)
		{
			CANDY_CORE_ERROR("DX12SwapChain: invalid window handle");
			return;
		}

		DXGI_SWAP_CHAIN_DESC1 scDesc = {};
		scDesc.Width              = desc.Width;
		scDesc.Height             = desc.Height;
		scDesc.Format             = format;
		scDesc.Stereo             = FALSE;
		scDesc.SampleDesc.Count   = 1;
		scDesc.SampleDesc.Quality = 0;
		scDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.BufferCount        = desc.BufferCount;
		scDesc.Scaling            = DXGI_SCALING_STRETCH;
		scDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		scDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
		scDesc.Flags              = desc.VSync ? 0u : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
		HRESULT hr = factory->CreateSwapChainForHwnd(
			queue, hwnd, &scDesc, nullptr, nullptr, &swapChain1);

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12SwapChain: CreateSwapChainForHwnd failed (HRESULT: {:#x})",
			                 static_cast<uint32_t>(hr));
			return;
		}

		hr = swapChain1.As(&m_SwapChain);
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12SwapChain: QueryInterface IDXGISwapChain3 failed");
			return;
		}

		// Create RTV descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = desc.BufferCount;
		rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask       = 0;

		hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12SwapChain: CreateDescriptorHeap (RTV) failed");
			return;
		}

		m_RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create back buffer RTVs
		CreateSwapChainResources();

		CANDY_CORE_INFO("DX12SwapChain: creation complete");
	}

	DX12SwapChain::~DX12SwapChain()
	{
		ReleaseSwapChainResources();
		CANDY_CORE_INFO("DX12SwapChain: destroyed");
	}

	void DX12SwapChain::CreateSwapChainResources()
	{
		m_BackBuffers.resize(m_Desc.BufferCount);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

		for (uint32_t i = 0; i < m_Desc.BufferCount; ++i)
		{
			HRESULT hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffers[i]));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("DX12SwapChain: GetBuffer({}) failed", i);
				continue;
			}

			m_Device->CreateRenderTargetView(m_BackBuffers[i].Get(), nullptr, rtvHandle);

			rtvHandle.ptr += m_RTVDescriptorSize;
		}

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void DX12SwapChain::ReleaseSwapChainResources()
	{
		for (auto& buffer : m_BackBuffers)
			buffer.Reset();
		m_BackBuffers.clear();
	}

	const SwapChainDesc& DX12SwapChain::GetDesc() const
	{
		return m_Desc;
	}

	Ref<RHITexture> DX12SwapChain::GetCurrentBackBuffer()
	{
		// TODO: wrap ID3D12Resource in DX12Texture when implemented
		CANDY_CORE_WARN("TODO: DX12SwapChain::GetCurrentBackBuffer — texture wrapper not implemented");
		return nullptr;
	}

	ID3D12Resource* DX12SwapChain::GetCurrentBackBufferResource() const
	{
		if (m_CurrentBackBufferIndex < m_BackBuffers.size())
			return m_BackBuffers[m_CurrentBackBufferIndex].Get();
		return nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::GetCurrentRTVHandle() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(m_CurrentBackBufferIndex) * m_RTVDescriptorSize;
		return handle;
	}

	void DX12SwapChain::AdvanceFrame()
	{
		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void DX12SwapChain::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_Desc.Width  = width;
		m_Desc.Height = height;

		// Release back buffer references before resize
		ReleaseSwapChainResources();

		CANDY_CORE_INFO("DX12SwapChain::Resize {}x{}", width, height);

		HRESULT hr = m_SwapChain->ResizeBuffers(
			m_Desc.BufferCount, width, height,
			DXGI_FORMAT_UNKNOWN,
			m_Desc.VSync ? 0u : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12SwapChain: ResizeBuffers failed (HRESULT: {:#x})",
			                 static_cast<uint32_t>(hr));
			return;
		}

		CreateSwapChainResources();
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
