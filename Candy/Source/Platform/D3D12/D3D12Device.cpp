#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12Buffer.h"
#include "Platform/D3D12/D3D12CommandBuffer.h"
#include "Platform/D3D12/D3D12SwapChain.h"
#include "Platform/D3D12/D3D12PipelineState.h"
#include "Platform/D3D12/D3D12Texture.h"
#include "Runtime/RHI/RHICommandQueue.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	// =========================================================================
	// Built-in triangle shader source (HLSL, compiled at runtime via D3DCompile)
	// =========================================================================
	static const char* kTriangleVS = R"(
cbuffer TransformCB : register(b0)
{
	float4x4 u_MVP;
};

struct VSInput
{
	float3 Position : POSITION;
	float4 Color    : COLOR;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color    : COLOR;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	output.Position = mul(u_MVP, float4(input.Position, 1.0));
	output.Color    = input.Color;
	return output;
}
)";

	static const char* kTrianglePS = R"(
struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color    : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
	return input.Color;
}
)";

	// =========================================================================
	// D3D12CommandQueue
	// =========================================================================
	class D3D12CommandQueue : public RHICommandQueue
	{
	public:
		D3D12CommandQueue(ID3D12Device* device, ComPtr<ID3D12CommandQueue> queue,
		                 ID3D12DescriptorHeap* cbvSrvUavHeap,
		                 ID3D12DescriptorHeap* samplerHeap)
			: m_Device(device), m_Queue(std::move(queue))
			, m_CBVSRVUAVHeap(cbvSrvUavHeap), m_SamplerHeap(samplerHeap)
		{
			HRESULT hr = device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator));
			if (FAILED(hr))
				CANDY_CORE_ERROR("D3D12CommandQueue: CreateCommandAllocator failed");
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
				CANDY_CORE_ERROR("D3D12CommandQueue: CreateCommandList failed");
				return nullptr;
			}

			cmdList->Close();

			return Candy::CreateScope<D3D12CommandBuffer>(
				std::move(cmdList), m_CommandAllocator.Get(), m_Device,
				m_CBVSRVUAVHeap, m_SamplerHeap);
		}

		void Submit(const std::vector<RHICommandBuffer*>& commandBuffers) override
		{
			std::vector<ID3D12CommandList*> nativeLists;
			nativeLists.reserve(commandBuffers.size());
			for (auto* cb : commandBuffers)
			{
				auto* d3d12cb = static_cast<D3D12CommandBuffer*>(cb);
				if (auto* list = d3d12cb->GetNativeCommandList())
					nativeLists.push_back(list);
			}
			if (!nativeLists.empty())
				m_Queue->ExecuteCommandLists(static_cast<UINT>(nativeLists.size()), nativeLists.data());
		}

		void Present(const Ref<RHISwapChain>& swapChain) override
		{
			auto* d3d12sc = dynamic_cast<D3D12SwapChain*>(swapChain.get());
			if (!d3d12sc)
			{
				CANDY_CORE_ERROR("D3D12CommandQueue::Present: not a D3D12SwapChain");
				return;
			}

			UINT syncInterval = d3d12sc->GetDesc().VSync ? 1u : 0u;
			UINT presentFlags = d3d12sc->GetDesc().VSync ? 0u : DXGI_PRESENT_ALLOW_TEARING;

			if (auto* sc = d3d12sc->GetSwapChain())
			{
				sc->Present(syncInterval, presentFlags);
				d3d12sc->AdvanceFrame();
			}
		}

		void WaitIdle() override
		{
			CANDY_CORE_WARN("TODO: D3D12CommandQueue::WaitIdle — use D3D12Device::WaitIdle");
		}

		[[nodiscard]] ID3D12CommandQueue*   GetNativeQueue()   const { return m_Queue.Get(); }
		[[nodiscard]] ID3D12CommandAllocator* GetAllocator()    const { return m_CommandAllocator.Get(); }

		void Signal(ID3D12Fence* fence, uint64_t value)
		{
			m_Queue->Signal(fence, value);
		}

	private:
		ID3D12Device*                  m_Device = nullptr;
		ComPtr<ID3D12CommandQueue>     m_Queue;
		ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
		ID3D12DescriptorHeap*          m_CBVSRVUAVHeap = nullptr;
		ID3D12DescriptorHeap*          m_SamplerHeap   = nullptr;
	};

	// =========================================================================
	// D3D12Device — Constructor
	// =========================================================================

	D3D12Device::D3D12Device()
	{
		CANDY_CORE_INFO("D3D12Device: initializing...");

#if defined(CANDY_DEBUG)
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			CANDY_CORE_INFO("D3D12Device: debug layer enabled");
		}
#endif

		// DXGI factory
		HRESULT hr = CreateDXGIFactory2(
#if defined(CANDY_DEBUG)
			DXGI_CREATE_FACTORY_DEBUG,
#else
			0,
#endif
			IID_PPV_ARGS(&m_Factory));

		if (FAILED(hr))
			hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_Factory));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device: failed to create DXGI factory");
			return;
		}

		// Adapter selection
		ComPtr<IDXGIAdapter1> adapter;
		for (UINT i = 0;
		     m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		                                           IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
		     ++i)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			                                _uuidof(ID3D12Device), nullptr)))
			{
				std::wstring wideName(desc.Description);
				std::string name(wideName.begin(), wideName.end());
				CANDY_CORE_INFO("D3D12Device: selected adapter '{}'", name);
				break;
			}
			adapter.Reset();
		}

		if (!adapter)
		{
			CANDY_CORE_WARN("D3D12Device: no high-performance adapter, fallback to first");
			m_Factory->EnumAdapters1(0, &adapter);
		}

		if (!adapter)
		{
			CANDY_CORE_ERROR("D3D12Device: no D3D12-capable adapter found");
			return;
		}

		// D3D12 device
		hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
		                       IID_PPV_ARGS(&m_NativeDevice));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device: D3D12CreateDevice failed");
			return;
		}

		// Descriptor heaps
		{
			D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
			cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			cbvHeapDesc.NumDescriptors = 256;
			cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			cbvHeapDesc.NodeMask       = 0;

			hr = m_NativeDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_CBVSRVUAVHeap));
			if (FAILED(hr))
				CANDY_CORE_ERROR("D3D12Device: CBV_SRV_UAV heap creation failed");
			else
				m_CBVSRVUAVDescriptorSize = m_NativeDevice->GetDescriptorHandleIncrementSize(
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
			samplerHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			samplerHeapDesc.NumDescriptors = 64;
			samplerHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			samplerHeapDesc.NodeMask       = 0;

			hr = m_NativeDevice->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_SamplerHeap));
			if (FAILED(hr))
				CANDY_CORE_ERROR("D3D12Device: Sampler heap creation failed");
			else
				m_SamplerDescriptorSize = m_NativeDevice->GetDescriptorHandleIncrementSize(
					D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}

		// Command queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;

		ComPtr<ID3D12CommandQueue> commandQueue;
		hr = m_NativeDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device: command queue creation failed");
			return;
		}

		m_CommandQueue = CreateScope<D3D12CommandQueue>(
			m_NativeDevice.Get(), std::move(commandQueue),
			m_CBVSRVUAVHeap.Get(), m_SamplerHeap.Get());

		// Fence
		hr = m_NativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device: fence creation failed");
			return;
		}

		m_FenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

		CANDY_CORE_INFO("D3D12Device: initialization complete");
	}

	D3D12Device::~D3D12Device()
	{
		WaitIdle();
		if (m_FenceEvent) CloseHandle(m_FenceEvent);
		CANDY_CORE_INFO("D3D12Device: shutdown complete");
	}

	// ---- Native accessors ---------------------------------------------------

	ID3D12CommandQueue* D3D12Device::GetNativeQueue() const
	{
		auto* q = static_cast<D3D12CommandQueue*>(m_CommandQueue.get());
		return q ? q->GetNativeQueue() : nullptr;
	}

	uint64_t D3D12Device::SignalFence()
	{
		++m_FenceValue;
		auto* q = static_cast<D3D12CommandQueue*>(m_CommandQueue.get());
		if (q) q->Signal(m_Fence.Get(), m_FenceValue);
		return m_FenceValue;
	}

	void D3D12Device::WaitForFenceValue(uint64_t value)
	{
		if (m_Fence->GetCompletedValue() < value)
		{
			m_Fence->SetEventOnCompletion(value, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}

	// ---- Shader compilation -------------------------------------------------

	ComPtr<ID3DBlob> D3D12Device::CompileHLSL(
		const char* source, const char* entryPoint,
		const char* target, const std::string& debugName)
	{
		ComPtr<ID3DBlob> bytecode;
		ComPtr<ID3DBlob> errors;

		UINT compileFlags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined(CANDY_DEBUG)
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		HRESULT hr = D3DCompile(
			source, strlen(source), debugName.c_str(),
			nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint, target, compileFlags, 0,
			&bytecode, &errors);

		if (FAILED(hr))
		{
			if (errors)
			{
				CANDY_CORE_ERROR("D3D12Device: HLSL compile error in '{}' ({}):\n{}",
				                 debugName, entryPoint,
				                 static_cast<const char*>(errors->GetBufferPointer()));
			}
			else
			{
				CANDY_CORE_ERROR("D3D12Device: HLSL compile failed for '{}' ({})",
				                 debugName, entryPoint);
			}
			return nullptr;
		}

		CANDY_CORE_INFO("D3D12Device: compiled '{}' ({}) — {} bytes",
		                debugName, target, bytecode->GetBufferSize());
		return bytecode;
	}

	const std::vector<uint8_t>& D3D12Device::GetTriangleVSBytecode()
	{
		if (m_TriangleVS.empty())
		{
			auto blob = CompileHLSL(kTriangleVS, "main", "vs_5_0", "TriangleVS");
			if (blob)
				m_TriangleVS.assign(
					static_cast<const uint8_t*>(blob->GetBufferPointer()),
					static_cast<const uint8_t*>(blob->GetBufferPointer()) + blob->GetBufferSize());
		}
		return m_TriangleVS;
	}

	const std::vector<uint8_t>& D3D12Device::GetTrianglePSBytecode()
	{
		if (m_TrianglePS.empty())
		{
			auto blob = CompileHLSL(kTrianglePS, "main", "ps_5_0", "TrianglePS");
			if (blob)
				m_TrianglePS.assign(
					static_cast<const uint8_t*>(blob->GetBufferPointer()),
					static_cast<const uint8_t*>(blob->GetBufferPointer()) + blob->GetBufferSize());
		}
		return m_TrianglePS;
	}

	// ---- Root signature -----------------------------------------------------

	ComPtr<ID3D12RootSignature> D3D12Device::CreateMinimalRootSignature()
	{
		// Root parameter 0: CBV (b0) — transform / per-draw constants
		D3D12_ROOT_PARAMETER rootParams[2] = {};

		// Parameter 0: CBV
		rootParams[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[0].Descriptor       = {};
		rootParams[0].Descriptor.ShaderRegister = 0;
		rootParams[0].Descriptor.RegisterSpace  = 0;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		// Parameter 1: Descriptor table with SRV (t0)
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors     = 1;
		srvRange.BaseShaderRegister = 0;
		srvRange.RegisterSpace      = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		rootParams[1].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// Static sampler (s0)
		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.MipLODBias       = 0.0f;
		staticSampler.MaxAnisotropy    = 1;
		staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
		staticSampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		staticSampler.MinLOD           = 0.0f;
		staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;
		staticSampler.ShaderRegister   = 0;
		staticSampler.RegisterSpace    = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
		rootSigDesc.NumParameters     = 2;
		rootSigDesc.pParameters       = rootParams;
		rootSigDesc.NumStaticSamplers = 1;
		rootSigDesc.pStaticSamplers   = &staticSampler;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			&signature, &error);

		if (FAILED(hr))
		{
			if (error)
				CANDY_CORE_ERROR("D3D12Device: RootSignature serialize error:\n{}",
				                 static_cast<const char*>(error->GetBufferPointer()));
			return nullptr;
		}

		ComPtr<ID3D12RootSignature> rootSig;
		hr = m_NativeDevice->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(),
			IID_PPV_ARGS(&rootSig));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device: CreateRootSignature failed");
			return nullptr;
		}

		return rootSig;
	}

	ComPtr<ID3D12RootSignature> D3D12Device::CreateTexturedRootSignature()
	{
		// Parameter 0: CBV (b0)
		// Parameter 1: descriptor table with 32 SRVs (t0-t31)
		D3D12_ROOT_PARAMETER rootParams[2] = {};

		rootParams[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[0].Descriptor       = {};
		rootParams[0].Descriptor.ShaderRegister = 0;
		rootParams[0].Descriptor.RegisterSpace  = 0;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors     = 32;
		srvRange.BaseShaderRegister = 0;
		srvRange.RegisterSpace      = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		rootParams[1].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// Static sampler (s0) — linear wrap
		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.MipLODBias       = 0.0f;
		staticSampler.MaxAnisotropy    = 1;
		staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
		staticSampler.MinLOD           = 0.0f;
		staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;
		staticSampler.ShaderRegister   = 0;
		staticSampler.RegisterSpace    = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
		rootSigDesc.NumParameters     = 2;
		rootSigDesc.pParameters       = rootParams;
		rootSigDesc.NumStaticSamplers = 1;
		rootSigDesc.pStaticSamplers   = &staticSampler;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			&signature, &error);

		if (FAILED(hr))
		{
			if (error)
				CANDY_CORE_ERROR("D3D12Device: TexturedRootSignature error:\n{}",
				                 static_cast<const char*>(error->GetBufferPointer()));
			return nullptr;
		}

		ComPtr<ID3D12RootSignature> rootSig;
		hr = m_NativeDevice->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(),
			IID_PPV_ARGS(&rootSig));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device: CreateTexturedRootSignature failed");
			return nullptr;
		}

		return rootSig;
	}

	// ---- Resource creation ---------------------------------------------------

	Ref<RHIBuffer> D3D12Device::CreateBuffer(const BufferDesc& desc)
	{
		return CreateRef<D3D12Buffer>(m_NativeDevice.Get(), desc);
	}

	Ref<RHITexture> D3D12Device::CreateTexture(const TextureDesc& desc)
	{
		return CreateRef<D3D12Texture>(m_NativeDevice.Get(), desc);
	}

	Ref<RHISampler> D3D12Device::CreateSampler(const SamplerDesc& desc)
	{
		return CreateRef<D3D12Sampler>(m_NativeDevice.Get(), m_SamplerHeap.Get(),
		                              m_SamplerDescriptorSize, desc);
	}

	Ref<RHIShaderModule> D3D12Device::CreateShaderModule(const void* bytecode, uint32_t byteSize, const std::string& debugName)
	{
		// Store the bytecode for pipeline creation — the actual shader module
		// is not a D3D12 runtime object; D3D12 pipelines consume bytecode directly.
		// We wrap it in a simple blob holder.
		struct D3D12ShaderModule : public RHIShaderModule
		{
			std::vector<uint8_t> Bytecode;
			ShaderStage          Stage = ShaderStage::None;
			std::string          Name;

			D3D12ShaderModule(const void* data, uint32_t size, ShaderStage stage, std::string_view name)
				: Stage(stage), Name(name)
			{
				auto* ptr = static_cast<const uint8_t*>(data);
				Bytecode.assign(ptr, ptr + size);
			}

			ShaderStage GetStage() const override { return Stage; }
			const uint32_t* GetBytecode() const override
			{
				return reinterpret_cast<const uint32_t*>(Bytecode.data());
			}
			uint32_t GetBytecodeSize() const override
			{
				return static_cast<uint32_t>(Bytecode.size());
			}
			const std::string& GetDebugName() const override { return Name; }
		};

		if (!bytecode || byteSize == 0)
		{
			CANDY_CORE_ERROR("D3D12Device::CreateShaderModule: null bytecode");
			return nullptr;
		}

		return CreateRef<D3D12ShaderModule>(bytecode, byteSize, ShaderStage::None, debugName);
	}

	Ref<RHIGraphicsPipeline> D3D12Device::CreateGraphicsPipeline(
		const GraphicsPipelineDesc& desc,
		const Ref<RHIShaderModule>& vs,
		const Ref<RHIShaderModule>& fs)
	{
		// Cache lookup
		if (auto cached = GetPipelineCache().Find(desc))
			return cached;

		// ---- Vertex input layout --------------------------------------------

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

		// Map RHIFormat → DXGI_FORMAT for common vertex attribute formats
		static const auto MapFormat = [](RHIFormat fmt) -> DXGI_FORMAT
		{
			switch (fmt)
			{
			case RHIFormat::R32G32Float:       return DXGI_FORMAT_R32G32_FLOAT;
			case RHIFormat::R32G32B32Float:    return DXGI_FORMAT_R32G32B32_FLOAT;
			case RHIFormat::R32G32B32A32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case RHIFormat::R8G8B8A8Unorm:     return DXGI_FORMAT_R8G8B8A8_UNORM;
			default:                           return DXGI_FORMAT_UNKNOWN;
			}
		};

		for (const auto& attr : desc.VertexInput.Attributes)
		{
			D3D12_INPUT_ELEMENT_DESC elem = {};
			// Use a single generic "TEXCOORD" semantic, with the attribute's
			// location used as the semantic index.  HLSL inputs must be
			// declared as TEXCOORD0..N to match this layout 1:1.
			elem.SemanticName         = "TEXCOORD";
			elem.SemanticIndex        = attr.Location;
			elem.Format               = MapFormat(attr.Format);
			elem.InputSlot            = attr.Binding;
			elem.AlignedByteOffset    = attr.Offset;
			elem.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			elem.InstanceDataStepRate = 0;

			inputElements.push_back(elem);
		}

		// ---- Root signature ------------------------------------------------

		auto rootSig = CreateMinimalRootSignature();
		if (!rootSig)
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGraphicsPipeline: root signature failed");
			return nullptr;
		}

		// ---- Rasterizer state ----------------------------------------------

		D3D12_RASTERIZER_DESC rasterizer = {};
		rasterizer.FillMode              = (desc.Rasterizer.Fill == FillMode::Wireframe)
		                                   ? D3D12_FILL_MODE_WIREFRAME
		                                   : D3D12_FILL_MODE_SOLID;
		rasterizer.CullMode              = (desc.Rasterizer.Cull == CullMode::None)  ? D3D12_CULL_MODE_NONE
		                                 : (desc.Rasterizer.Cull == CullMode::Front) ? D3D12_CULL_MODE_FRONT
		                                                                              : D3D12_CULL_MODE_BACK;
		rasterizer.FrontCounterClockwise = FALSE;
		rasterizer.DepthBias             = desc.Rasterizer.DepthBias;
		rasterizer.DepthBiasClamp        = desc.Rasterizer.DepthBiasClamp;
		rasterizer.SlopeScaledDepthBias  = desc.Rasterizer.DepthBiasSlopeFactor;
		rasterizer.DepthClipEnable       = desc.Rasterizer.DepthClipEnable;
		rasterizer.MultisampleEnable     = desc.SampleCount > 1;

		// ---- Depth-stencil ------------------------------------------------

		D3D12_DEPTH_STENCIL_DESC depthStencil = {};
		depthStencil.DepthEnable      = desc.DepthStencil.DepthTestEnable;
		depthStencil.DepthWriteMask   = desc.DepthStencil.DepthWriteEnable
		                                ? D3D12_DEPTH_WRITE_MASK_ALL
		                                : D3D12_DEPTH_WRITE_MASK_ZERO;
		// Map CompareOp → D3D12_COMPARISON_FUNC
		static const D3D12_COMPARISON_FUNC compMap[] = {
			D3D12_COMPARISON_FUNC_NEVER,        // Never
			D3D12_COMPARISON_FUNC_LESS,          // Less
			D3D12_COMPARISON_FUNC_EQUAL,         // Equal
			D3D12_COMPARISON_FUNC_LESS_EQUAL,    // LessEqual
			D3D12_COMPARISON_FUNC_GREATER,       // Greater
			D3D12_COMPARISON_FUNC_NOT_EQUAL,     // NotEqual
			D3D12_COMPARISON_FUNC_GREATER_EQUAL, // GreaterEqual
			D3D12_COMPARISON_FUNC_ALWAYS         // Always
		};
		depthStencil.DepthFunc = compMap[static_cast<int>(desc.DepthStencil.DepthCompareOp)];

		// ---- Blend state ---------------------------------------------------

		D3D12_BLEND_DESC blend = {};
		blend.RenderTarget[0].BlendEnable   = desc.Blend.BlendEnable;
		blend.RenderTarget[0].SrcBlend      = D3D12_BLEND_SRC_ALPHA;
		blend.RenderTarget[0].DestBlend     = D3D12_BLEND_INV_SRC_ALPHA;
		blend.RenderTarget[0].BlendOp       = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		blend.RenderTarget[0].BlendOpAlpha  = D3D12_BLEND_OP_ADD;

		// Color write mask
		UINT8 writeMask = 0;
		if (HasFlag(desc.Blend.WriteMask, ColorWriteMask::Red))   writeMask |= D3D12_COLOR_WRITE_ENABLE_RED;
		if (HasFlag(desc.Blend.WriteMask, ColorWriteMask::Green)) writeMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
		if (HasFlag(desc.Blend.WriteMask, ColorWriteMask::Blue))  writeMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
		if (HasFlag(desc.Blend.WriteMask, ColorWriteMask::Alpha)) writeMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
		blend.RenderTarget[0].RenderTargetWriteMask = writeMask;

		// ---- PSO description ----------------------------------------------

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature        = rootSig.Get();
		psoDesc.VS                    = { vs->GetBytecode(), vs->GetBytecodeSize() };
		psoDesc.PS                    = { fs->GetBytecode(), fs->GetBytecodeSize() };
		psoDesc.BlendState            = blend;
		psoDesc.SampleMask            = UINT_MAX;
		psoDesc.RasterizerState       = rasterizer;
		psoDesc.DepthStencilState     = depthStencil;
		psoDesc.InputLayout           = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
		psoDesc.PrimitiveTopologyType = (desc.Topology == PrimitiveTopology::Lines ||
		                                 desc.Topology == PrimitiveTopology::LineStrip)
		                                ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
		                                : (desc.Topology == PrimitiveTopology::Points)
		                                  ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT
		                                  : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets      = desc.RenderTargetFormats.empty()
		                                 ? 1u
		                                 : static_cast<UINT>(desc.RenderTargetFormats.size());

		// Map RHIFormats → RTVFormats[i] (driver requires one entry per RT
		// declared on the PS output signature; falling short triggers PSO
		// creation failure on pipelines that write SV_TARGET1, etc.).
		static const auto MapRenderTargetFormat = [](RHIFormat fmt) -> DXGI_FORMAT
		{
			switch (fmt)
			{
			case RHIFormat::B8G8R8A8Unorm:    return DXGI_FORMAT_B8G8R8A8_UNORM;
			case RHIFormat::B8G8R8A8Srgb:     return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case RHIFormat::R8G8B8A8Unorm:    return DXGI_FORMAT_R8G8B8A8_UNORM;
			case RHIFormat::R32Sint:          return DXGI_FORMAT_R32_SINT;
			default:                          return DXGI_FORMAT_R8G8B8A8_UNORM;
			}
		};

		if (desc.RenderTargetFormats.empty())
		{
			psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
		}
		else
		{
			for (UINT i = 0; i < psoDesc.NumRenderTargets; ++i)
				psoDesc.RTVFormats[i] = MapRenderTargetFormat(desc.RenderTargetFormats[i]);
		}

		if (desc.DepthStencilFormat != RHIFormat::Unknown)
		{
			// Map depth format
			switch (desc.DepthStencilFormat)
			{
			case RHIFormat::D32Float:       psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; break;
			case RHIFormat::D24UnormS8Uint:  psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
			default:                         psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; break;
			}
		}
		else
		{
			psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}

		psoDesc.SampleDesc.Count   = desc.SampleCount;
		psoDesc.SampleDesc.Quality = 0;

		ComPtr<ID3D12PipelineState> pso;
		HRESULT hr = m_NativeDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGraphicsPipeline: CreateGraphicsPipelineState failed");
			return nullptr;
		}

		auto pipeline = CreateRef<D3D12GraphicsPipeline>(desc);
		pipeline->SetNativePipeline(std::move(pso), std::move(rootSig));

		GetPipelineCache().Insert(desc, pipeline);

		CANDY_CORE_INFO("D3D12Device::CreateGraphicsPipeline: pipeline created ({} attributes, {} RT format(s))",
		                desc.VertexInput.Attributes.size(), desc.RenderTargetFormats.size());

		return pipeline;
	}

	Ref<RHIGraphicsPipeline> D3D12Device::CreateGraphicsPipelineWithRootSig(
		const GraphicsPipelineDesc& desc,
		const Ref<RHIShaderModule>& vs, const Ref<RHIShaderModule>& fs,
		ID3D12RootSignature* rootSig)
	{
		if (!rootSig)
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGraphicsPipelineWithRootSig: null root signature");
			return nullptr;
		}

		// Build D3D12 pipeline state (same as CreateGraphicsPipeline but with custom root sig)
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
		static const auto MapFormat = [](RHIFormat fmt) -> DXGI_FORMAT
		{
			switch (fmt)
			{
			case RHIFormat::R32G32Float:       return DXGI_FORMAT_R32G32_FLOAT;
			case RHIFormat::R32G32B32Float:    return DXGI_FORMAT_R32G32B32_FLOAT;
			case RHIFormat::R32G32B32A32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case RHIFormat::R8G8B8A8Unorm:     return DXGI_FORMAT_R8G8B8A8_UNORM;
			case RHIFormat::R32Float:          return DXGI_FORMAT_R32_FLOAT;
			case RHIFormat::R32Sint:           return DXGI_FORMAT_R32_SINT;
			default:                           return DXGI_FORMAT_UNKNOWN;
			}
		};

		for (const auto& attr : desc.VertexInput.Attributes)
		{
			D3D12_INPUT_ELEMENT_DESC elem = {};
			// Same generic TEXCOORD<N> convention as CreateGraphicsPipeline.
			elem.SemanticName         = "TEXCOORD";
			elem.SemanticIndex        = attr.Location;
			elem.Format               = MapFormat(attr.Format);
			elem.InputSlot            = attr.Binding;
			elem.AlignedByteOffset    = attr.Offset;
			elem.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			elem.InstanceDataStepRate = 0;

			inputElements.push_back(elem);
		}

		D3D12_RASTERIZER_DESC rasterizer = {};
		rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizer.CullMode = D3D12_CULL_MODE_NONE;

		D3D12_DEPTH_STENCIL_DESC depthStencil = {};
		depthStencil.DepthEnable    = desc.DepthStencil.DepthTestEnable;
		depthStencil.DepthWriteMask = desc.DepthStencil.DepthWriteEnable
			? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;

		D3D12_BLEND_DESC blend = {};
		blend.RenderTarget[0].BlendEnable   = desc.Blend.BlendEnable;
		blend.RenderTarget[0].SrcBlend      = D3D12_BLEND_SRC_ALPHA;
		blend.RenderTarget[0].DestBlend     = D3D12_BLEND_INV_SRC_ALPHA;
		blend.RenderTarget[0].BlendOp       = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		blend.RenderTarget[0].BlendOpAlpha  = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature        = rootSig;
		psoDesc.VS                    = { vs->GetBytecode(), vs->GetBytecodeSize() };
		psoDesc.PS                    = { fs->GetBytecode(), fs->GetBytecodeSize() };
		psoDesc.BlendState            = blend;
		psoDesc.SampleMask            = UINT_MAX;
		psoDesc.RasterizerState       = rasterizer;
		psoDesc.DepthStencilState     = depthStencil;
		psoDesc.InputLayout           = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
		psoDesc.PrimitiveTopologyType = (desc.Topology == PrimitiveTopology::Lines ||
		                                 desc.Topology == PrimitiveTopology::LineStrip)
		                                ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
		                                : (desc.Topology == PrimitiveTopology::Points)
		                                  ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT
		                                  : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets      = desc.RenderTargetFormats.empty()
		                                 ? 1u
		                                 : static_cast<UINT>(desc.RenderTargetFormats.size());

		// Same RTV-format mapping as CreateGraphicsPipeline so both paths
		// honour the desc rather than baking in a fixed 2-render-target layout.
		static const auto MapRTFormatRootSig = [](RHIFormat fmt) -> DXGI_FORMAT
		{
			switch (fmt)
			{
			case RHIFormat::B8G8R8A8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
			case RHIFormat::B8G8R8A8Srgb:  return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case RHIFormat::R8G8B8A8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case RHIFormat::R32Sint:       return DXGI_FORMAT_R32_SINT;
			default:                       return DXGI_FORMAT_R8G8B8A8_UNORM;
			}
		};

		if (desc.RenderTargetFormats.empty())
		{
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			for (UINT i = 0; i < psoDesc.NumRenderTargets; ++i)
				psoDesc.RTVFormats[i] = MapRTFormatRootSig(desc.RenderTargetFormats[i]);
		}

		psoDesc.DSVFormat     = DXGI_FORMAT_UNKNOWN;
		psoDesc.SampleDesc.Count = 1;

		ComPtr<ID3D12PipelineState> pso;
		HRESULT hr = m_NativeDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGraphicsPipelineWithRootSig: CreateGraphicsPipelineState failed");
			return nullptr;
		}

		auto pipeline = CreateRef<D3D12GraphicsPipeline>(desc);
		pipeline->SetNativePipeline(std::move(pso), rootSig);
		return pipeline;
	}

	Ref<RHISwapChain> D3D12Device::CreateSwapChain(const SwapChainDesc& desc)
	{
		return CreateRef<D3D12SwapChain>(
			m_NativeDevice.Get(), m_Factory.Get(),
			GetNativeQueue(), desc);
	}

	// ---- Command submission -------------------------------------------------

	RHICommandQueue& D3D12Device::GetCommandQueue()
	{
		return *m_CommandQueue;
	}

	void D3D12Device::WaitIdle()
	{
		uint64_t fenceValue = SignalFence();
		WaitForFenceValue(fenceValue);
	}

	// ---- Vertex buffer helpers -----------------------------------------------

	Ref<RHIBuffer> D3D12Device::CreateTriangleVertexBuffer()
	{
		// Single triangle: 3 vertices, each Position(3 floats) + Color(4 floats)
		VertexPosColor vertices[] = {
			{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // top    — red
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // left   — green
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, // right  — blue
		};

		return CreateGPUBufferWithData(vertices, sizeof(vertices),
		                               ResourceUsage::VertexBuffer, "TriangleVB");
	}

	Ref<RHIBuffer> D3D12Device::CreateTriangleIndexBuffer()
	{
		uint32_t indices[] = { 0, 1, 2 };
		return CreateGPUBufferWithData(indices, sizeof(indices),
		                               ResourceUsage::IndexBuffer, "TriangleIB");
	}

	Ref<RHIBuffer> D3D12Device::CreateVertexBufferWithData(const void* data, uint64_t size,
	                                                      std::string_view debugName)
	{
		return CreateGPUBufferWithData(data, size, ResourceUsage::VertexBuffer, debugName);
	}

	Ref<RHIBuffer> D3D12Device::CreateGPUBufferWithData(const void* data, uint64_t size,
	                                                   ResourceUsage usage,
	                                                   std::string_view debugName)
	{
		if (!data || size == 0)
			return nullptr;

		// 1. Create upload (staging) buffer
		BufferDesc uploadDesc;
		uploadDesc.Size          = size;
		uploadDesc.Usage         = ResourceUsage::CopySrc;
		uploadDesc.CPUAccessible = true;
		uploadDesc.DebugName     = std::string(debugName) + "_Upload";

		auto uploadBuffer = CreateRef<D3D12Buffer>(m_NativeDevice.Get(), uploadDesc);
		if (!uploadBuffer->GetResource())
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGPUBufferWithData: upload buffer creation failed");
			return nullptr;
		}

		// Map and copy data
		void* mapped = uploadBuffer->Map();
		memcpy(mapped, data, static_cast<size_t>(size));
		uploadBuffer->Unmap();

		// 2. Create default-heap (GPU) buffer
		BufferDesc gpuDesc;
		gpuDesc.Size          = size;
		gpuDesc.Usage         = usage | ResourceUsage::CopyDst;
		gpuDesc.CPUAccessible = false;
		gpuDesc.DebugName     = std::string(debugName);

		auto gpuBuffer = CreateRef<D3D12Buffer>(m_NativeDevice.Get(), gpuDesc);
		if (!gpuBuffer->GetResource())
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGPUBufferWithData: GPU buffer creation failed");
			return nullptr;
		}

		// 3. Use a temporary command list to copy upload → GPU
		ComPtr<ID3D12CommandAllocator> tempAllocator;
		HRESULT hr = m_NativeDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGPUBufferWithData: temp allocator failed");
			return nullptr;
		}

		ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		hr = m_NativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		                                       tempAllocator.Get(), nullptr,
		                                       IID_PPV_ARGS(&tempCmdList));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Device::CreateGPUBufferWithData: temp cmd list failed");
			return nullptr;
		}

		// Copy
		tempCmdList->CopyResource(gpuBuffer->GetResource(), uploadBuffer->GetResource());

		// Transition gpu buffer to target usage state
		D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_COMMON;
		if (HasFlag(usage, ResourceUsage::VertexBuffer))
			targetState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		else if (HasFlag(usage, ResourceUsage::IndexBuffer))
			targetState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		else if (HasFlag(usage, ResourceUsage::ConstantBuffer))
			targetState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		if (targetState != D3D12_RESOURCE_STATE_COMMON)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource   = gpuBuffer->GetResource();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			barrier.Transition.StateAfter  = targetState;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			tempCmdList->ResourceBarrier(1, &barrier);
		}

		tempCmdList->Close();

		// Execute and wait
		ID3D12CommandList* lists[] = { tempCmdList.Get() };
		GetNativeQueue()->ExecuteCommandLists(1, lists);

		uint64_t fenceVal = SignalFence();
		WaitForFenceValue(fenceVal);

		gpuBuffer->SetState(targetState);
		return gpuBuffer;
	}

	Ref<RHIBuffer> D3D12Device::CreateIdentityMVPBuffer()
	{
		// 4x4 row-major identity matrix (64 bytes)
		float identity[16] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		return CreateGPUBufferWithData(identity, sizeof(identity),
		                               ResourceUsage::ConstantBuffer, "IdentityMVP");
	}

} // namespace Candy
