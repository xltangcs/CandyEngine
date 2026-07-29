#pragma once

#include "Runtime/RHI/RHITypes.h"
#include "Runtime/Core/Base.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Candy::IR {

	// =========================================================================
	// ResourceType — coarse categorization used by the resource manager
	// =========================================================================
	enum class ResourceType : uint8_t
	{
		Unknown = 0,
		Buffer,
		Texture,
		Sampler,
		ShaderModule,
		GraphicsPipeline,
		SwapChain,
		Fence,
		Semaphore,
		CommandBuffer
	};

	// =========================================================================
	// ResourceState — tracks resource usage for automatic barrier generation
	// =========================================================================
	enum class ResourceState : uint32_t
	{
		Undefined      = 0,
		VertexBuffer   = 1 << 0,
		IndexBuffer    = 1 << 1,
		ConstantBuffer = 1 << 2,
		ShaderRead     = 1 << 3,
		ShaderWrite    = 1 << 4,
		RenderTarget   = 1 << 5,
		DepthWrite     = 1 << 6,
		DepthRead      = 1 << 7,
		CopySrc        = 1 << 8,
		CopyDst        = 1 << 9,
		Present        = 1 << 10
	};
	CANDY_DEFINE_ENUM_FLAG_OPERATORS(ResourceState)

	// =========================================================================
	// MemoryType — GPU memory heap category
	//
	// GPUOnly     → DEVICE_LOCAL — fastest, CPU-inaccessible
	// CPUUpload   → HOST_VISIBLE | HOST_COHERENT — staging / upload
	// CPUReadback → HOST_VISIBLE | HOST_CACHED   — readback
	// =========================================================================
	enum class MemoryType : uint8_t
	{
		GPUOnly,
		CPUUpload,
		CPUReadback
	};

	// =========================================================================
	// DescriptorSetLayoutDesc — maps to VkDescriptorSetLayout / root params
	// =========================================================================
	struct DescriptorSetLayoutDesc
	{
		struct Binding
		{
			uint32_t     Slot       = 0;
			uint32_t     Count      = 1;
			ResourceType Type       = ResourceType::Unknown;
			Candy::ShaderStage StageFlags = Candy::ShaderStage::None;
		};

		uint32_t             SetIndex = 0;
		std::vector<Binding> Bindings;
	};

	// =========================================================================
	// BindingSlot — (set, binding) pair used as a uniform resource key
	// =========================================================================
	struct BindingSlot
	{
		uint32_t Set     = 0;
		uint32_t Binding = 0;

		bool operator==(const BindingSlot&) const = default;
	};

} // namespace Candy::IR

// =========================================================================
// std::hash specializations for RHI types used as map keys in IR
// =========================================================================
template<>
struct std::hash<Candy::RHIHandle>
{
	size_t operator()(const Candy::RHIHandle& h) const noexcept
	{
		return std::hash<uint32_t>{}(h.Value);
	}
};

template<>
struct std::hash<Candy::IR::BindingSlot>
{
	size_t operator()(const Candy::IR::BindingSlot& s) const noexcept
	{
		return (static_cast<uint64_t>(s.Set) << 32) | static_cast<uint64_t>(s.Binding);
	}
};
