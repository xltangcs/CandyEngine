#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Platform/D3D12/D3D12SwapChain.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	D3D12SwapChain::D3D12SwapChain(ID3D12Device* device, IDXGIFactory6* factory,
	                             ID3D12CommandQueue* queue, const SwapChainDesc& desc)
		: m_Desc(desc), m_Device(device), m_Factory(factory), m_Queue(queue)
	{
		CANDY_CORE_INFO("D3D12SwapChain: creating {}x{} (buffer count: {}, vsync: {})",
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
			CANDY_CORE_ERROR("D3D12SwapChain: invalid window handle");
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
		// NOTE: the creation flags must be passed back to ResizeBuffers, so cache
		// them here.
		m_CreationFlags = desc.VSync ? 0u : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		scDesc.Flags              = m_CreationFlags;

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
		HRESULT hr = factory->CreateSwapChainForHwnd(
			queue, hwnd, &scDesc, nullptr, nullptr, &swapChain1);

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12SwapChain: CreateSwapChainForHwnd failed (HRESULT: {:#x})",
			                 static_cast<uint32_t>(hr));
			return;
		}

		hr = swapChain1.As(&m_SwapChain);
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12SwapChain: QueryInterface IDXGISwapChain3 failed");
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
			CANDY_CORE_ERROR("D3D12SwapChain: CreateDescriptorHeap (RTV) failed");
			return;
		}

		m_RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create resize fence (used to drain in-flight presents in Resize()).
		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_ResizeFence))))
			CANDY_CORE_ERROR("D3D12SwapChain: CreateFence (resize) failed");
		m_ResizeFenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

		// Create back buffer RTVs
		CreateSwapChainResources();

		CANDY_CORE_INFO("D3D12SwapChain: creation complete");
	}

	D3D12SwapChain::~D3D12SwapChain()
	{
		ReleaseSwapChainResources();
		if (m_ResizeFenceEvent)
		{
			CloseHandle(m_ResizeFenceEvent);
			m_ResizeFenceEvent = nullptr;
		}
		CANDY_CORE_INFO("D3D12SwapChain: destroyed");
	}

	void D3D12SwapChain::CreateSwapChainResources()
	{
		m_BackBuffers.resize(m_Desc.BufferCount);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

		for (uint32_t i = 0; i < m_Desc.BufferCount; ++i)
		{
			HRESULT hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffers[i]));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("D3D12SwapChain: GetBuffer({}) failed", i);
				continue;
			}

			m_Device->CreateRenderTargetView(m_BackBuffers[i].Get(), nullptr, rtvHandle);

			rtvHandle.ptr += m_RTVDescriptorSize;
		}

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void D3D12SwapChain::ReleaseSwapChainResources()
	{
		for (auto& buffer : m_BackBuffers)
			buffer.Reset();
		m_BackBuffers.clear();
	}

	const SwapChainDesc& D3D12SwapChain::GetDesc() const
	{
		return m_Desc;
	}

	Ref<RHITexture> D3D12SwapChain::GetCurrentBackBuffer()
	{
		// TODO: wrap ID3D12Resource in D3D12Texture when implemented
		CANDY_CORE_WARN("TODO: D3D12SwapChain::GetCurrentBackBuffer — texture wrapper not implemented");
		return nullptr;
	}

	ID3D12Resource* D3D12SwapChain::GetCurrentBackBufferResource() const
	{
		if (m_CurrentBackBufferIndex < m_BackBuffers.size())
			return m_BackBuffers[m_CurrentBackBufferIndex].Get();
		return nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12SwapChain::GetCurrentRTVHandle() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(m_CurrentBackBufferIndex) * m_RTVDescriptorSize;
		return handle;
	}

	void D3D12SwapChain::AdvanceFrame()
	{
		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void D3D12SwapChain::SignalAfterPresent()
	{
		// Signal the resize fence right after Present so a subsequent Resize()
		// can drain the displayed frame. This mirrors the Microsoft D3D12 sample
		// pattern (Present → Signal → wait on Signal before ResizeBuffers).
		if (m_Queue && m_ResizeFence)
			m_Queue->Signal(m_ResizeFence.Get(), ++m_ResizeFenceValue);
	}

	void D3D12SwapChain::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_Desc.Width  = width;
		m_Desc.Height = height;

		// Wait for the last presented frame to be fully consumed before
		// releasing the back buffers. SignalAfterPresent() signals this fence on
		// the queue *after* Present, so it covers both the command list and the
		// display-pipeline present. ResizeBuffers on a flip-model swapchain with
		// a still-pending present returns DXGI_ERROR_DEVICE_REMOVED (0x887A0005).
		if (m_Queue && m_ResizeFence && m_ResizeFenceEvent)
		{
			UINT64 target = m_ResizeFenceValue;
			if (m_ResizeFence->GetCompletedValue() < target)
			{
				m_ResizeFence->SetEventOnCompletion(target, m_ResizeFenceEvent);
				// Bounded wait: a removed device never signals the fence.
				if (WaitForSingleObject(m_ResizeFenceEvent, 5000) != WAIT_OBJECT_0)
					CANDY_CORE_ERROR("D3D12SwapChain::Resize: timed out waiting for present fence");
			}
		}

		// Release back buffer references before resize
		ReleaseSwapChainResources();

		CANDY_CORE_INFO("D3D12SwapChain::Resize {}x{}", width, height);

		HRESULT hr = m_SwapChain->ResizeBuffers(
			m_Desc.BufferCount, width, height,
			DXGI_FORMAT_UNKNOWN,
			m_CreationFlags);

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12SwapChain: ResizeBuffers failed (HRESULT: {:#x})",
			                 static_cast<uint32_t>(hr));
			if (m_Device)
			{
				HRESULT removed = m_Device->GetDeviceRemovedReason();
				CANDY_CORE_ERROR("D3D12SwapChain: GetDeviceRemovedReason = {:#x}",
				                 static_cast<uint32_t>(removed));
			}
			return;
		}

		CreateSwapChainResources();
	}

	uint32_t D3D12SwapChain::GetWidth() const
	{
		return m_Desc.Width;
	}

	uint32_t D3D12SwapChain::GetHeight() const
	{
		return m_Desc.Height;
	}

} // namespace Candy
