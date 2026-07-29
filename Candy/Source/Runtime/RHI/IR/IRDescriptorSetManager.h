#pragma once

#include "Runtime/RHI/IR/IRTypes.h"

#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Candy::IR {

	// =========================================================================
	// IRDescriptorSetManager — manages descriptor set pool allocation and
	// descriptor set layout management.
	//
	// Vulkan:   maps to VkDescriptorPool + VkDescriptorSetLayout + VkDescriptorSet
	// DX12:     maps to descriptor heap management + root parameter ranges
	// =========================================================================
	class IRDescriptorSetManager
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

		IRDescriptorSetManager() = default;
		~IRDescriptorSetManager();

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
	};

} // namespace Candy::IR
