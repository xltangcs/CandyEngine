#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Platform/DX12/DX12Device.h"
#include "Platform/DX12/DX12Buffer.h"
#include "Platform/DX12/DX12CommandBuffer.h"
#include "Platform/DX12/DX12SwapChain.h"
#include "Platform/DX12/DX12PipelineState.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	// =========================================================================
	// DX12CommandQueue — wraps ID3D12CommandQueue for submission / present
	// =========================================================================
	class DX12CommandQueue : public RHICommandQueue
	{
	public:
		DX12CommandQueue(ID3D12Device* device, ComPtr<ID3D12CommandQueue> queue)
			: m_Device(device), m_Queue(std::move(queue))
		{
			// Create a command allocator (reused for simplicity)
			HRESULT hr = device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator));
			if (FAILED(hr))
				CANDY_CORE_ERROR("DX12CommandQueue: CreateCommandAllocator failed");
		}

		Scope<RHICommandBuffer> CreateCommandBuffer() override
		{
			ComPtr<ID3D12GraphicsCommandList> cmdList;
			HRESULT hr = m_Device->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				m_CommandAllocator.Get(), nullptr,
				IID_PPV_ARGS(&cmdList));

			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("DX12CommandQueue: CreateCommandList failed");
				return nullptr;
			}

			// Command list is created in recording state; close it initially
			cmdList->Close();

			return Candy::CreateScope<DX12CommandBuffer>(
				std::move(cmdList), m_CommandAllocator.Get());
		}

		void Submit(const std::vector<RHICommandBuffer*>& commandBuffers) override
		{
			std::vector<ID3D12CommandList*> nativeLists;
			nativeLists.reserve(commandBuffers.size());

			for (auto* cb : commandBuffers)
			{
				auto* dx12cb = static_cast<DX12CommandBuffer*>(cb);
				if (auto* list = dx12cb->GetNativeCommandList())
					nativeLists.push_back(list);
			}

			if (!nativeLists.empty())
				m_Queue->ExecuteCommandLists(static_cast<UINT>(nativeLists.size()), nativeLists.data());
		}

		void Present(const Ref<RHISwapChain>& swapChain) override
		{
			auto* dx12sc = dynamic_cast<DX12SwapChain*>(swapChain.get());
			if (!dx12sc)
			{
				CANDY_CORE_ERROR("DX12CommandQueue::Present: not a DX12SwapChain");
				return;
			}

			UINT syncInterval = dx12sc->GetDesc().VSync ? 1u : 0u;
			UINT presentFlags = dx12sc->GetDesc().VSync ? 0u : DXGI_PRESENT_ALLOW_TEARING;

			IDXGISwapChain3* sc = dx12sc->GetSwapChain();
			if (sc)
			{
				sc->Present(syncInterval, presentFlags);
				dx12sc->AdvanceFrame();
			}
		}

		void WaitIdle() override
		{
			CANDY_CORE_WARN("TODO: DX12CommandQueue::WaitIdle — use DX12Device::WaitIdle instead");
		}

		[[nodiscard]] ID3D12CommandQueue* GetNativeQueue() const { return m_Queue.Get(); }
		[[nodiscard]] ID3D12CommandAllocator* GetAllocator() const { return m_CommandAllocator.Get(); }

		/// Signal a fence on this queue
		void Signal(ID3D12Fence* fence, uint64_t value)
		{
			m_Queue->Signal(fence, value);
		}

	private:
		ID3D12Device*                       m_Device = nullptr;
		ComPtr<ID3D12CommandQueue>          m_Queue;
		ComPtr<ID3D12CommandAllocator>      m_CommandAllocator;
	};

	// =========================================================================
	// DX12Device
	// =========================================================================

	DX12Device::DX12Device()
	{
		CANDY_CORE_INFO("DX12Device: initializing...");

#if defined(CANDY_DEBUG)
		// Enable the D3D12 debug layer
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			CANDY_CORE_INFO("DX12Device: debug layer enabled");
		}
#endif

		// Create DXGI factory
		HRESULT hr = CreateDXGIFactory2(
#if defined(CANDY_DEBUG)
			DXGI_CREATE_FACTORY_DEBUG,
#else
			0,
#endif
			IID_PPV_ARGS(&m_Factory));

		if (FAILED(hr))
		{
			hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_Factory));
		}

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Device: failed to create DXGI factory");
			return;
		}

		// Select adapter — prefer high-performance GPU
		ComPtr<IDXGIAdapter1> adapter;
		for (UINT i = 0;
		     m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		                                           IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
		     ++i)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			                                _uuidof(ID3D12Device), nullptr)))
			{
				std::wstring wideName(desc.Description);
				std::string name(wideName.begin(), wideName.end());
				CANDY_CORE_INFO("DX12Device: selected adapter '{}'", name);
				break;
			}

			adapter.Reset();
		}

		if (!adapter)
		{
			CANDY_CORE_WARN("DX12Device: no high-performance adapter, falling back to first adapter");
			m_Factory->EnumAdapters1(0, &adapter);
		}

		if (!adapter)
		{
			CANDY_CORE_ERROR("DX12Device: no D3D12-capable adapter found");
			return;
		}

		// Create D3D12 device
		hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
		                       IID_PPV_ARGS(&m_NativeDevice));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Device: D3D12CreateDevice failed");
			return;
		}

		// Create command queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		ComPtr<ID3D12CommandQueue> commandQueue;
		hr = m_NativeDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Device: failed to create command queue");
			return;
		}

		m_CommandQueue = CreateScope<DX12CommandQueue>(m_NativeDevice.Get(), std::move(commandQueue));

		// Create fence for synchronization
		hr = m_NativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Device: failed to create fence");
			return;
		}

		m_FenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
		if (!m_FenceEvent)
		{
			CANDY_CORE_ERROR("DX12Device: failed to create fence event");
			return;
		}

		CANDY_CORE_INFO("DX12Device: initialization complete");
	}

	DX12Device::~DX12Device()
	{
		WaitIdle();

		if (m_FenceEvent)
			CloseHandle(m_FenceEvent);

		CANDY_CORE_INFO("DX12Device: shutdown complete");
	}

	// ---- Native accessor helpers ---------------------------------------------

	ID3D12CommandQueue* DX12Device::GetNativeQueue() const
	{
		auto* q = static_cast<DX12CommandQueue*>(m_CommandQueue.get());
		return q ? q->GetNativeQueue() : nullptr;
	}

	uint64_t DX12Device::SignalFence()
	{
		++m_FenceValue;
		auto* q = static_cast<DX12CommandQueue*>(m_CommandQueue.get());
		if (q)
			q->Signal(m_Fence.Get(), m_FenceValue);
		return m_FenceValue;
	}

	void DX12Device::WaitForFenceValue(uint64_t value)
	{
		if (m_Fence->GetCompletedValue() < value)
		{
			m_Fence->SetEventOnCompletion(value, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}

	// ---- Resource creation ---------------------------------------------------

	Ref<RHIBuffer> DX12Device::CreateBuffer(const BufferDesc& desc)
	{
		return CreateRef<DX12Buffer>(m_NativeDevice.Get(), desc);
	}

	Ref<RHITexture> DX12Device::CreateTexture(const TextureDesc& desc)
	{
		CANDY_CORE_WARN("TODO: DX12Device::CreateTexture — not yet implemented");
		return nullptr;
	}

	Ref<RHISampler> DX12Device::CreateSampler(const SamplerDesc& desc)
	{
		CANDY_CORE_WARN("TODO: DX12Device::CreateSampler — not yet implemented");
		return nullptr;
	}

	Ref<RHIShaderModule> DX12Device::CreateShaderModule(const void* spirvBytecode, uint32_t byteSize, const std::string& debugName)
	{
		CANDY_CORE_WARN("TODO: DX12Device::CreateShaderModule — SPIR-V→DXIL compilation needed");
		return nullptr;
	}

	Ref<RHIGraphicsPipeline> DX12Device::CreateGraphicsPipeline(
		const GraphicsPipelineDesc& desc,
		const Ref<RHIShaderModule>& vs,
		const Ref<RHIShaderModule>& fs)
	{
		if (auto cached = GetPipelineCache().Find(desc))
			return cached;

		CANDY_CORE_WARN("TODO: DX12Device::CreateGraphicsPipeline — not yet implemented");

		Ref<DX12GraphicsPipeline> pipeline = CreateRef<DX12GraphicsPipeline>(desc);
		GetPipelineCache().Insert(desc, pipeline);
		return pipeline;
	}

	Ref<RHISwapChain> DX12Device::CreateSwapChain(const SwapChainDesc& desc)
	{
		return CreateRef<DX12SwapChain>(
			m_NativeDevice.Get(), m_Factory.Get(),
			GetNativeQueue(), desc);
	}

	// ---- Command submission --------------------------------------------------

	RHICommandQueue& DX12Device::GetCommandQueue()
	{
		return *m_CommandQueue;
	}

	void DX12Device::WaitIdle()
	{
		uint64_t fenceValue = SignalFence();
		WaitForFenceValue(fenceValue);
	}

} // namespace Candy
