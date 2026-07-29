#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Platform/DX12/DX12Device.h"
#include "Platform/DX12/DX12CommandBuffer.h"
#include "Platform/DX12/DX12SwapChain.h"
#include "Platform/DX12/DX12PipelineState.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	// ---- Queue implementation ------------------------------------------------

	class DX12CommandQueue : public RHICommandQueue
	{
	public:
		DX12CommandQueue(ComPtr<ID3D12CommandQueue> queue)
			: m_Queue(std::move(queue)) {}

		Scope<RHICommandBuffer> CreateCommandBuffer() override
		{
			CANDY_CORE_WARN("TODO: DX12CommandQueue::CreateCommandBuffer — not yet implemented");
			return Scope<DX12CommandBuffer>(new DX12CommandBuffer());
		}

		void Submit(const std::vector<RHICommandBuffer*>& commandBuffers) override
		{
			CANDY_CORE_WARN("TODO: DX12CommandQueue::Submit — not yet implemented");
		}

		void Present(const Ref<RHISwapChain>& swapChain) override
		{
			CANDY_CORE_WARN("TODO: DX12CommandQueue::Present — not yet implemented");
		}

		void WaitIdle() override
		{
			CANDY_CORE_WARN("TODO: DX12CommandQueue::WaitIdle — not yet implemented");
		}

		[[nodiscard]] ID3D12CommandQueue* GetNativeQueue() const { return m_Queue.Get(); }

	private:
		ComPtr<ID3D12CommandQueue> m_Queue;
	};

	// ---- DX12 Device ---------------------------------------------------------

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
		ComPtr<IDXGIFactory6> factory;
		HRESULT hr = CreateDXGIFactory2(
#if defined(CANDY_DEBUG)
			DXGI_CREATE_FACTORY_DEBUG,
#else
			0,
#endif
			IID_PPV_ARGS(&factory));

		if (FAILED(hr))
		{
			// Fallback: try without debug flag
			hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
		}

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Device: failed to create DXGI factory");
			return;
		}

		// Select adapter — prefer high-performance GPU
		ComPtr<IDXGIAdapter1> adapter;
		for (UINT i = 0;
		     factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		                                         IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
		     ++i)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Skip software adapters
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			// Check if D3D12 is supported
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
			CANDY_CORE_WARN("DX12Device: no high-performance adapter found, falling back to first adapter");
			factory->EnumAdapters1(0, &adapter);
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

		m_CommandQueue = CreateScope<DX12CommandQueue>(std::move(commandQueue));

		CANDY_CORE_INFO("DX12Device: initialization complete");
	}

	DX12Device::~DX12Device()
	{
		WaitIdle();
		CANDY_CORE_INFO("DX12Device: shutdown complete");
	}

	// ---- Resource creation ---------------------------------------------------

	Ref<RHIBuffer> DX12Device::CreateBuffer(const BufferDesc& desc)
	{
		CANDY_CORE_WARN("TODO: DX12Device::CreateBuffer — not yet implemented");
		return nullptr;
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
		// Check cache first
		if (auto cached = GetPipelineCache().Find(desc))
			return cached;

		CANDY_CORE_WARN("TODO: DX12Device::CreateGraphicsPipeline — not yet implemented");

		// Create a placeholder pipeline and cache it
		Ref<DX12GraphicsPipeline> pipeline = CreateRef<DX12GraphicsPipeline>(desc);
		GetPipelineCache().Insert(desc, pipeline);
		return pipeline;
	}

	Ref<RHISwapChain> DX12Device::CreateSwapChain(const SwapChainDesc& desc)
	{
		CANDY_CORE_INFO("DX12Device::CreateSwapChain {}x{}", desc.Width, desc.Height);
		return CreateRef<DX12SwapChain>(desc);
	}

	// ---- Command submission --------------------------------------------------

	RHICommandQueue& DX12Device::GetCommandQueue()
	{
		return *m_CommandQueue;
	}

	void DX12Device::WaitIdle()
	{
		if (auto* q = static_cast<DX12CommandQueue*>(m_CommandQueue.get()))
			q->WaitIdle();
	}

} // namespace Candy
