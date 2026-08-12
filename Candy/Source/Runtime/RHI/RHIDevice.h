#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHIPipelineState.h"
#include "Runtime/RHI/RHISwapChain.h"

#include <cstdint>
#include <string>

namespace Candy {

	// =========================================================================
	// Buffer descriptor
	// =========================================================================
	struct BufferDesc
	{
		uint64_t      Size  = 0;
		ResourceUsage Usage = ResourceUsage::None;
		/// Preferred memory type (e.g. CPU-accessible vs GPU-local).  A value
		/// of `true` means CPU-visible (upload / readback).
		bool          CPUAccessible = false;
		/// Stride per vertex when Usage includes VertexBuffer.  Ignored for
		/// index/constant/storage buffers.
		uint32_t      Stride = 0;
		std::string   DebugName;
	};

	// =========================================================================
	// Texture descriptor
	// =========================================================================
	struct TextureDesc
	{
		uint32_t      Width      = 1;
		uint32_t      Height     = 1;
		uint32_t      Depth      = 1;
		uint32_t      MipLevels  = 1;
		uint32_t      ArrayLayers = 1;
		RHIFormat     Format     = RHIFormat::R8G8B8A8Unorm;
		ResourceUsage Usage      = ResourceUsage::ShaderRead;
		uint32_t      SampleCount = 1;
		std::string   DebugName;
	};

	// =========================================================================
	// Sampler descriptor
	// =========================================================================
	struct SamplerDesc
	{
		SamplerFilter      MinFilter    = SamplerFilter::Linear;
		SamplerFilter      MagFilter    = SamplerFilter::Linear;
		SamplerFilter      MipFilter    = SamplerFilter::Linear;
		SamplerAddressMode AddressU     = SamplerAddressMode::Repeat;
		SamplerAddressMode AddressV     = SamplerAddressMode::Repeat;
		SamplerAddressMode AddressW     = SamplerAddressMode::Repeat;
		CompareOp          CompareOp    = CompareOp::Never; ///< Never → regular sampler
		float              MinLod       = 0.0f;
		float              MaxLod       = 16.0f;
		float              MipLodBias   = 0.0f;
		uint32_t           MaxAnisotropy = 1;
		std::string        DebugName;
	};

	// =========================================================================
	// RHIBuffer — GPU buffer resource (vertex, index, constant, storage, …)
	// =========================================================================
	class RHIBuffer
	{
	public:
		virtual ~RHIBuffer() = default;

		virtual const BufferDesc& GetDesc() const = 0;

		/// Map the buffer to CPU-accessible memory.  Must have been created
		/// with CPUAccessible = true.
		virtual void* Map()    = 0;
		virtual void  Unmap()  = 0;
	};

	// =========================================================================
	// RHITexture — 1D / 2D / 3D / Cubemap texture resource
	// =========================================================================
	class RHITexture
	{
	public:
		virtual ~RHITexture() = default;

		virtual const TextureDesc& GetDesc() const = 0;
	};

	// =========================================================================
	// RHISampler — texture sampler state object
	// =========================================================================
	class RHISampler
	{
	public:
		virtual ~RHISampler() = default;

		virtual const SamplerDesc& GetDesc() const = 0;
	};

	// =========================================================================
	// RHIDevice — logical device, the primary GPU resource factory
	// =========================================================================
	class RHIShaderModule;
	class RHISwapChain;
	class RHICommandQueue;

	class RHIDevice
	{
	public:
		virtual ~RHIDevice() = default;

		// ---- Resource creation ----------------------------------------------

		virtual Ref<RHIBuffer>   CreateBuffer(const BufferDesc& desc) = 0;
		virtual Ref<RHITexture>  CreateTexture(const TextureDesc& desc) = 0;
		virtual Ref<RHISampler>  CreateSampler(const SamplerDesc& desc) = 0;

		virtual Ref<RHIShaderModule> CreateShaderModule(const void* spirvBytecode, uint32_t byteSize, const std::string& debugName = "") = 0;

		virtual Ref<RHIGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const Ref<RHIShaderModule>& vs, const Ref<RHIShaderModule>& fs) = 0;

		virtual Ref<RHISwapChain> CreateSwapChain(const SwapChainDesc& desc) = 0;

		// ---- Command submission --------------------------------------------

		virtual RHICommandQueue& GetCommandQueue() = 0;

		// ---- Query ---------------------------------------------------------

		/// Wait until the GPU has finished all submitted work.
		virtual void WaitIdle() = 0;
	};

} // namespace Candy
