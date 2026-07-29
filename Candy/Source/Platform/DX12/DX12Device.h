#pragma once

#include "Runtime/RHI/IR/IRDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>

namespace Candy {

	// =========================================================================
	// DX12Device — Direct3D 12 backend
	//
	// Owns ID3D12Device, IDXGIFactory6, command queue, fence, descriptor heaps,
	// and a built-in shader cache for inline HLSL compilation.
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

		Candy::Ref<RHIShaderModule> CreateShaderModule(const void* bytecode, uint32_t byteSize, const std::string& debugName = "") override;

		Candy::Ref<RHIGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const Candy::Ref<RHIShaderModule>& vs, const Candy::Ref<RHIShaderModule>& fs) override;

		/// Create a graphics pipeline with a specific root signature (for textured batch rendering).
		Candy::Ref<RHIGraphicsPipeline> CreateGraphicsPipelineWithRootSig(
			const GraphicsPipelineDesc& desc,
			const Candy::Ref<RHIShaderModule>& vs, const Candy::Ref<RHIShaderModule>& fs,
			ID3D12RootSignature* rootSig);

		Candy::Ref<RHISwapChain> CreateSwapChain(const SwapChainDesc& desc) override;

		// ---- Command submission --------------------------------------------

		RHICommandQueue& GetCommandQueue() override;

		// ---- Query ---------------------------------------------------------

		void WaitIdle() override;

		// ---- DX12-specific native accessors --------------------------------

		[[nodiscard]] ID3D12Device*        GetNativeDevice()       const { return m_NativeDevice.Get(); }
		[[nodiscard]] IDXGIFactory6*       GetNativeFactory()      const { return m_Factory.Get(); }
		[[nodiscard]] ID3D12CommandQueue*  GetNativeQueue()        const;

		[[nodiscard]] ID3D12DescriptorHeap* GetCBVSRVUAVHeap()     const { return m_CBVSRVUAVHeap.Get(); }
		[[nodiscard]] ID3D12DescriptorHeap* GetSamplerHeap()       const { return m_SamplerHeap.Get(); }
		[[nodiscard]] uint32_t              GetCBVSRVDescriptorSize() const { return m_CBVSRVUAVDescriptorSize; }
		[[nodiscard]] uint32_t              GetSamplerDescriptorSize() const { return m_SamplerDescriptorSize; }

		// ---- Synchronization -----------------------------------------------

		uint64_t SignalFence();
		void     WaitForFenceValue(uint64_t value);

		// ---- Shader compilation --------------------------------------------

		/// Compile HLSL source to DXBC bytecode at runtime.
		/// Returns a blob suitable for CreateShaderModule / pipeline creation.
		Microsoft::WRL::ComPtr<ID3DBlob> CompileHLSL(
			const char* source, const char* entryPoint,
			const char* target, const std::string& debugName = "");

		/// Create a minimal root signature for a simple triangle pipeline.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateMinimalRootSignature();

		/// Create a root signature for textured 2D rendering (CBV + 32 SRV + sampler).
		/// Parameter 0: CBV (b0), Parameter 1: descriptor table 32 SRV (t0-t31), Static sampler (s0).
		Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateTexturedRootSignature();

		/// Get the built-in triangle VS bytecode (compiled on first call).
		const std::vector<uint8_t>& GetTriangleVSBytecode();
		/// Get the built-in triangle PS bytecode (compiled on first call).
		const std::vector<uint8_t>& GetTrianglePSBytecode();

		// ---- Triangle vertex data ------------------------------------------

		struct VertexPosColor
		{
			float Position[3];
			float Color[4];
		};

		/// Create a vertex buffer with a single colored triangle (upload heap).
		Candy::Ref<RHIBuffer> CreateTriangleVertexBuffer();
		/// Create an index buffer for the triangle (optional, for DrawIndexed).
		Candy::Ref<RHIBuffer> CreateTriangleIndexBuffer();
		/// Create a vertex buffer from arbitrary data (upload heap, CPU-accessible).
		Candy::Ref<RHIBuffer> CreateVertexBufferWithData(const void* data, uint64_t size,
		                                                 std::string_view debugName = "");
		/// Create a default-heap buffer and upload data via a staging buffer.
		Candy::Ref<RHIBuffer> CreateGPUBufferWithData(const void* data, uint64_t size,
		                                              ResourceUsage usage,
		                                              std::string_view debugName = "");
		/// Create a constant buffer with an identity MVP matrix (64 bytes, aligned to 256).
		Candy::Ref<RHIBuffer> CreateIdentityMVPBuffer();

	private:
		Microsoft::WRL::ComPtr<ID3D12Device>        m_NativeDevice;
		Microsoft::WRL::ComPtr<IDXGIFactory6>       m_Factory;
		Candy::Scope<RHICommandQueue>               m_CommandQueue;

		// Descriptor heaps
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_CBVSRVUAVHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SamplerHeap;
		uint32_t m_CBVSRVUAVDescriptorSize = 0;
		uint32_t m_SamplerDescriptorSize   = 0;

		// Synchronization
		Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
		uint64_t                            m_FenceValue = 0;
		HANDLE                              m_FenceEvent  = nullptr;

		// Built-in shader cache
		std::vector<uint8_t> m_TriangleVS;
		std::vector<uint8_t> m_TrianglePS;
	};

} // namespace Candy
