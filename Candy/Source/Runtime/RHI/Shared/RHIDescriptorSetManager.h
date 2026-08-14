#pragma once

#include "Runtime/RHI/Shared/RHISharedTypes.h"

#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Candy {

	// =========================================================================
	// RHIDescriptorSetManager — manages descriptor set pool allocation and
	// descriptor set layout management.
	//
	// Vulkan:   maps to VkDescriptorPool + VkDescriptorSetLayout + VkDescriptorSet
	// D3D12:     maps to descriptor heap management + root parameter ranges
	// =========================================================================
	class RHIDescriptorSetManager
	{
	public:
		/// Describes the maximum capacity of a descriptor pool.
		struct PoolDesc
		{
			uint32_t MaxSets              = 256;
			uint32_t MaxConstantBuffers   = 256;
			uint32_t MaxSampledImages     = 256;
			uint32_t MaxStorageBuffers    = 64;
			uint32_t MaxStorageImages     = 64;
			uint32_t MaxSamplers          = 64;
		};

		/// Describes a single descriptor write operation.
		struct DescriptorWrite
		{
			Candy::RHIHandle SetHandle       {};
			uint32_t         Binding         = 0;
			ResourceType     Type            = ResourceType::Unknown;
			Candy::RHIHandle ResourceHandle  {}; ///< handle of the resource to bind
			uint32_t         ArrayIndex      = 0;
		};

		RHIDescriptorSetManager() = default;
		~RHIDescriptorSetManager();

		// ---- Layout management ---------------------------------------------

		/// Register a descriptor set layout; returns a layout handle.
		Candy::RHIHandle RegisterLayout(const DescriptorSetLayoutDesc& desc);
		void             UnregisterLayout(Candy::RHIHandle layoutHandle);

		[[nodiscard]] const DescriptorSetLayoutDesc* GetLayout(Candy::RHIHandle handle) const;

		// ---- Set allocation ------------------------------------------------

		/// Allocate a descriptor set matching the given layout.
		/// Returns a handle that identifies the set for subsequent writes.
		Candy::RHIHandle AllocateSet(Candy::RHIHandle layoutHandle);
		void             FreeSet(Candy::RHIHandle setHandle);

		// ---- Pool management -----------------------------------------------

		void InitPool(const PoolDesc& desc);
		void ResetPool();

		// ---- Descriptor writes ---------------------------------------------

		/// Queues a descriptor write (applied on CommitWrites()).
		void WriteDescriptor(const DescriptorWrite& write);

		/// Flushes all pending descriptor writes.
		void CommitWrites();

		/// Discards all pending descriptor writes without applying them.
		void DiscardWrites();

		// ---- Linear range allocator (D3D12 shared descriptor heap) ---------

		/// Initialize the linear allocator with the heap's total descriptor count.
		void InitRangeAllocator(uint32_t capacity);

		/// Allocate a contiguous descriptor range; returns the base slot.
		/// Asserts on exhaustion. D3D12 uses this to hand out regions of the
		/// shared CBV_SRV_UAV heap (texture table, ImGui fonts, framebuffer
		/// SRVs, engine textures) instead of hard-coded slot constants.
		uint32_t AllocateRange(uint32_t count);

		[[nodiscard]] uint32_t GetRangeUsed()     const { return m_NextRangeSlot; }
		[[nodiscard]] uint32_t GetRangeCapacity() const { return m_RangeCapacity; }

		// ---- Query ---------------------------------------------------------

		[[nodiscard]] size_t GetPendingWriteCount() const { return m_PendingWrites.size(); }
		[[nodiscard]] size_t GetAllocatedSetCount()  const { return m_SetLayouts.size(); }

	private:
		PoolDesc m_PoolDesc;

		std::vector<DescriptorWrite>                           m_PendingWrites;
		std::unordered_map<Candy::RHIHandle, DescriptorSetLayoutDesc> m_Layouts;

		// Tracking: setHandle → layoutHandle
		std::unordered_map<Candy::RHIHandle, Candy::RHIHandle> m_SetLayouts;

		uint32_t m_NextLayoutHandle = 1;
		uint32_t m_NextSetHandle    = 1;

		// Linear range allocator state (D3D12 shared heap)
		uint32_t m_RangeCapacity = 0;
		uint32_t m_NextRangeSlot = 0;
	};

} // namespace Candy
