#pragma once

#include "Runtime/RHI/IR/IRTypes.h"

#include <cstdint>
#include <vector>

namespace Candy::IR {

	// =========================================================================
	// IRMemoryAllocator — GPU memory sub-allocation using a slab / free-list
	// strategy.
	//
	// Instead of calling the driver API for every small buffer or texture,
	// the allocator acquires large GPU memory blocks and sub-allocates from
	// them, drastically reducing driver overhead.
	//
	// Backends (Vulkan / DX12) fill in the actual memory handles:
	//   Vulkan → VkDeviceMemory
	//   DX12   → ID3D12Heap*
	// =========================================================================
	class IRMemoryAllocator
	{
	public:
		/// A sub-allocation within a larger memory block.
		struct Allocation
		{
			void*    Data       = nullptr; ///< mapped CPU pointer (if CPU-accessible)
			uint64_t Offset     = 0;       ///< byte offset within parent block
			uint64_t Size       = 0;
			uint64_t BlockIndex = 0;       ///< which large block this belongs to
		};

		/// Default block size for GPU-local allocations (256 MiB).
		static constexpr uint64_t DefaultGPUBlockSize = 256ULL * 1024 * 1024;

		/// Default block size for CPU-accessible allocations (64 MiB).
		static constexpr uint64_t DefaultCPUBlockSize =  64ULL * 1024 * 1024;

		IRMemoryAllocator() = default;
		~IRMemoryAllocator();

		// ---- Configuration -------------------------------------------------

		void SetBlockSizes(uint64_t gpuBlockSize, uint64_t cpuBlockSize);

		// ---- Allocation ----------------------------------------------------

		/// Allocate memory.  'size' must be > 0; 'alignment' must be a power of 2.
		[[nodiscard]] Allocation Allocate(uint64_t size, uint64_t alignment, MemoryType memoryType);

		/// Free a previously allocated region (coalesces adjacent free regions).
		void Free(const Allocation& alloc);

		/// Release all blocks and reset the allocator.
		void Reset();

		// ---- Statistics ----------------------------------------------------

		[[nodiscard]] uint64_t GetTotalAllocated(MemoryType type) const;
		[[nodiscard]] uint64_t GetTotalUsed(MemoryType type) const;
		[[nodiscard]] uint64_t GetBlockCount(MemoryType type) const;

		// ---- Backend integration -------------------------------------------

		struct MemoryBlock
		{
			uint64_t   Size      = 0;
			uint64_t   Used      = 0;
			MemoryType Type      = MemoryType::GPUOnly;
			void*      RawHandle = nullptr; ///< VkDeviceMemory / ID3D12Heap*
		};

		/// Provides access to the block list for backend initialization.
		[[nodiscard]] std::vector<MemoryBlock>&       GetBlocks()       { return m_Blocks; }
		[[nodiscard]] const std::vector<MemoryBlock>& GetBlocks() const { return m_Blocks; }

	private:
		struct FreeRegion
		{
			uint64_t Offset = 0;
			uint64_t Size   = 0;
		};

		uint64_t m_GPUBlockSize = DefaultGPUBlockSize;
		uint64_t m_CPUBlockSize = DefaultCPUBlockSize;

		std::vector<MemoryBlock> m_Blocks;
		std::vector<FreeRegion>  m_FreeRegions; ///< sorted by offset, coalesced on free
	};

} // namespace Candy::IR
