#include "CandyPCH.h"
#include "Runtime/RHI/Shared/RHIShared.h"

#include "Runtime/Core/Log.h"

#include <algorithm>
#include <cstring>
#include <functional>

namespace Candy {

// =========================================================================
// RHIResourceManager
// =========================================================================

RHIResourceManager::~RHIResourceManager()
{
	if (!m_Resources.empty())
		CANDY_CORE_WARN("RHIResourceManager: {} resources still registered on shutdown", m_Resources.size());
}

Candy::RHIHandle RHIResourceManager::Register(ResourceType type, void* rawPtr, std::string_view name)
{
	Candy::RHIHandle handle{ m_NextHandle++ };
	ResourceEntry entry;
	entry.Type    = type;
	entry.State   = ResourceState::Undefined;
	entry.Name    = std::string(name);
	entry.RawPtr  = rawPtr;
	m_Resources[handle] = std::move(entry);
	return handle;
}

void RHIResourceManager::Unregister(Candy::RHIHandle handle)
{
	auto it = m_Resources.find(handle);
	if (it == m_Resources.end())
	{
		CANDY_CORE_ERROR("RHIResourceManager::Unregister: handle {} not found", handle.Value);
		return;
	}
	m_Resources.erase(it);
}

const RHIResourceManager::ResourceEntry* RHIResourceManager::Find(Candy::RHIHandle handle) const
{
	auto it = m_Resources.find(handle);
	return it != m_Resources.end() ? &it->second : nullptr;
}

RHIResourceManager::ResourceEntry* RHIResourceManager::Find(Candy::RHIHandle handle)
{
	auto it = m_Resources.find(handle);
	return it != m_Resources.end() ? &it->second : nullptr;
}

ResourceType RHIResourceManager::GetType(Candy::RHIHandle handle) const
{
	if (auto* e = Find(handle)) return e->Type;
	return ResourceType::Unknown;
}

ResourceState RHIResourceManager::GetState(Candy::RHIHandle handle) const
{
	if (auto* e = Find(handle)) return e->State;
	return ResourceState::Undefined;
}

void RHIResourceManager::SetState(Candy::RHIHandle handle, ResourceState newState)
{
	if (auto* e = Find(handle)) e->State = newState;
	else CANDY_CORE_ERROR("RHIResourceManager::SetState: handle {} not found", handle.Value);
}

bool RHIResourceManager::IsRegistered(Candy::RHIHandle handle) const
{
	return m_Resources.find(handle) != m_Resources.end();
}

// =========================================================================
// RHIPipelineCache
// =========================================================================

RHIPipelineCache::~RHIPipelineCache() = default;

Candy::Ref<Candy::RHIGraphicsPipeline>
RHIPipelineCache::Find(const Candy::GraphicsPipelineDesc& desc) const
{
	size_t h = HashDesc(desc);
	auto it = m_Cache.find(h);
	return it != m_Cache.end() ? it->second : nullptr;
}

bool RHIPipelineCache::Insert(const Candy::GraphicsPipelineDesc& desc,
                             const Candy::Ref<Candy::RHIGraphicsPipeline>& pipeline)
{
	size_t h = HashDesc(desc);
	if (m_Cache.find(h) != m_Cache.end())
		return false;
	m_Cache[h] = pipeline;
	return true;
}

void RHIPipelineCache::Erase(const Candy::GraphicsPipelineDesc& desc)
{
	m_Cache.erase(HashDesc(desc));
}

void RHIPipelineCache::Clear()
{
	m_Cache.clear();
}

// HashCombine helper — folds each field into an accumulator.
// Using std::hash plus a golden-ratio multiplier (good distribution).
static void HashCombine(size_t& seed, size_t value)
{
	seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

size_t RHIPipelineCache::HashDesc(const Candy::GraphicsPipelineDesc& desc)
{
	size_t seed = 0;

	// VertexInput
	for (const auto& a : desc.VertexInput.Attributes)
	{
		HashCombine(seed, std::hash<uint32_t>{}(a.Location));
		HashCombine(seed, std::hash<uint32_t>{}(a.Binding));
		HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(a.Format)));
		HashCombine(seed, std::hash<uint32_t>{}(a.Offset));
	}
	for (const auto& b : desc.VertexInput.Bindings)
	{
		HashCombine(seed, std::hash<uint32_t>{}(b.Binding));
		HashCombine(seed, std::hash<uint32_t>{}(b.Stride));
		HashCombine(seed, std::hash<bool>{}(b.PerInstance));
	}

	// Rasterizer
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Rasterizer.Cull)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Rasterizer.Fill)));
	HashCombine(seed, std::hash<bool>{}(desc.Rasterizer.DepthClipEnable));
	HashCombine(seed, std::hash<int32_t>{}(desc.Rasterizer.DepthBias));
	HashCombine(seed, std::hash<float>{}(desc.Rasterizer.DepthBiasSlopeFactor));
	HashCombine(seed, std::hash<float>{}(desc.Rasterizer.DepthBiasClamp));

	// DepthStencil
	HashCombine(seed, std::hash<bool>{}(desc.DepthStencil.DepthTestEnable));
	HashCombine(seed, std::hash<bool>{}(desc.DepthStencil.DepthWriteEnable));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.DepthStencil.DepthCompareOp)));

	// Blend
	HashCombine(seed, std::hash<bool>{}(desc.Blend.BlendEnable));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.WriteMask)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.SrcColorBlendFactor)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.DstColorBlendFactor)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.ColorBlendOp)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.SrcAlphaBlendFactor)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.DstAlphaBlendFactor)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Blend.AlphaBlendOp)));

	// Misc
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Topology)));
	for (auto fmt : desc.RenderTargetFormats)
		HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(fmt)));
	HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.DepthStencilFormat)));
	HashCombine(seed, std::hash<uint32_t>{}(desc.SampleCount));

	return seed;
}

// =========================================================================
// RHICommandValidator
// =========================================================================
// All methods are inlined/compiled-out in the header under CANDY_RELEASE.
// In Debug they validate state transitions with assertions.

void RHICommandValidator::OnBegin()
{
	CANDY_CORE_ASSERT(!m_Recording, "RHICommandValidator: Begin() called while already recording");
	m_Recording = true;
	m_InRenderPass   = false;
	m_PipelineSet    = false;
	m_VertexBufferBound = false;
	m_IndexBufferBound  = false;
}

void RHICommandValidator::OnEnd()
{
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: End() called without Begin()");
	CANDY_CORE_ASSERT(!m_InRenderPass, "RHICommandValidator: End() called inside a render pass");
	m_Recording = false;
}

void RHICommandValidator::OnBeginRenderPass(const Candy::RenderPassDesc& desc)
{
	(void)desc;
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: BeginRenderPass() called outside recording");
	CANDY_CORE_ASSERT(!m_InRenderPass, "RHICommandValidator: BeginRenderPass() called while already in a pass");
	m_InRenderPass = true;
}

void RHICommandValidator::OnEndRenderPass()
{
	CANDY_CORE_ASSERT(m_InRenderPass, "RHICommandValidator: EndRenderPass() called without BeginRenderPass()");
	m_InRenderPass = false;
}

void RHICommandValidator::OnSetPipeline(const Candy::Ref<Candy::RHIGraphicsPipeline>& pipeline)
{
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: SetPipeline() called outside recording");
	m_PipelineSet = (pipeline != nullptr);
}

void RHICommandValidator::OnSetVertexBuffer(uint32_t slot)
{
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: SetVertexBuffer() called outside recording");
	(void)slot;
	m_VertexBufferBound = true;
}

void RHICommandValidator::OnSetIndexBuffer()
{
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: SetIndexBuffer() called outside recording");
	m_IndexBufferBound = true;
}

void RHICommandValidator::OnDraw(uint32_t vertexCount)
{
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: Draw() called outside recording");
	CANDY_CORE_ASSERT(m_InRenderPass, "RHICommandValidator: Draw() called outside render pass");
	CANDY_CORE_ASSERT(m_PipelineSet, "RHICommandValidator: Draw() called without SetPipeline()");
	(void)vertexCount;
}

void RHICommandValidator::OnDrawIndexed(uint32_t indexCount)
{
	CANDY_CORE_ASSERT(m_Recording, "RHICommandValidator: DrawIndexed() called outside recording");
	CANDY_CORE_ASSERT(m_InRenderPass, "RHICommandValidator: DrawIndexed() called outside render pass");
	CANDY_CORE_ASSERT(m_PipelineSet, "RHICommandValidator: DrawIndexed() called without SetPipeline()");
	CANDY_CORE_ASSERT(m_IndexBufferBound, "RHICommandValidator: DrawIndexed() called without SetIndexBuffer()");
	(void)indexCount;
}

// =========================================================================
// RHIDescriptorSetManager
// =========================================================================

RHIDescriptorSetManager::~RHIDescriptorSetManager() = default;

Candy::RHIHandle RHIDescriptorSetManager::RegisterLayout(const DescriptorSetLayoutDesc& desc)
{
	Candy::RHIHandle h{ m_NextLayoutHandle++ };
	m_Layouts[h] = desc;
	return h;
}

void RHIDescriptorSetManager::UnregisterLayout(Candy::RHIHandle layoutHandle)
{
	m_Layouts.erase(layoutHandle);
}

const DescriptorSetLayoutDesc* RHIDescriptorSetManager::GetLayout(Candy::RHIHandle handle) const
{
	auto it = m_Layouts.find(handle);
	return it != m_Layouts.end() ? &it->second : nullptr;
}

Candy::RHIHandle RHIDescriptorSetManager::AllocateSet(Candy::RHIHandle layoutHandle)
{
	if (m_Layouts.find(layoutHandle) == m_Layouts.end())
	{
		CANDY_CORE_ERROR("RHIDescriptorSetManager::AllocateSet: layout handle {} not found", layoutHandle.Value);
		return {};
	}
	Candy::RHIHandle setH{ m_NextSetHandle++ };
	m_SetLayouts[setH] = layoutHandle;
	return setH;
}

void RHIDescriptorSetManager::FreeSet(Candy::RHIHandle setHandle)
{
	m_SetLayouts.erase(setHandle);
}

void RHIDescriptorSetManager::InitPool(const PoolDesc& desc)
{
	m_PoolDesc = desc;
	m_PendingWrites.clear();
}

void RHIDescriptorSetManager::ResetPool()
{
	m_PendingWrites.clear();
	m_Layouts.clear();
	m_SetLayouts.clear();
	m_NextLayoutHandle = 1;
	m_NextSetHandle    = 1;
}

void RHIDescriptorSetManager::WriteDescriptor(const DescriptorWrite& write)
{
	m_PendingWrites.push_back(write);
}

void RHIDescriptorSetManager::CommitWrites()
{
	// Backend-agnostic: we only record pending writes here. The concrete
	// backend (Vulkan/D3D12) is responsible for draining m_PendingWrites
	// and translating them to vkUpdateDescriptorSets / descriptor heap copies.
	// Once drained, clear the queue.
	m_PendingWrites.clear();
}

void RHIDescriptorSetManager::DiscardWrites()
{
	m_PendingWrites.clear();
}

void RHIDescriptorSetManager::InitRangeAllocator(uint32_t capacity)
{
	m_RangeCapacity = capacity;
	m_NextRangeSlot = 0;
}

uint32_t RHIDescriptorSetManager::AllocateRange(uint32_t count)
{
	uint32_t base = m_NextRangeSlot;
	// NOTE: CANDY_CORE_ASSERT only supports (check) or (check, msg-literal) —
	// its GET_MACRO arg-count dispatch breaks with 3+ args, so no format args.
	CANDY_CORE_ASSERT(base + count <= m_RangeCapacity,
	                  "RHIDescriptorSetManager: descriptor range exhausted");
	m_NextRangeSlot += count;
	return base;
}

// =========================================================================
// RHIMemoryAllocator
// =========================================================================

RHIMemoryAllocator::~RHIMemoryAllocator()
{
	Reset();
}

void RHIMemoryAllocator::SetBlockSizes(uint64_t gpuBlockSize, uint64_t cpuBlockSize)
{
	m_GPUBlockSize = gpuBlockSize;
	m_CPUBlockSize = cpuBlockSize;
}

RHIMemoryAllocator::Allocation RHIMemoryAllocator::Allocate(uint64_t size, uint64_t alignment, MemoryType memoryType)
{
	if (size == 0 || (alignment & (alignment - 1)) != 0)
	{
		CANDY_CORE_ERROR("RHIMemoryAllocator::Allocate: invalid size={} alignment={}", size, alignment);
		return {};
	}

	uint64_t blockSize = (memoryType == MemoryType::CPUUpload || memoryType == MemoryType::CPUReadback)
	                       ? m_CPUBlockSize : m_GPUBlockSize;

	// First-fit search through existing free regions.
	for (size_t i = 0; i < m_FreeRegions.size(); ++i)
	{
		auto& region = m_FreeRegions[i];
		// Align the region offset up to `alignment`.
		uint64_t alignedOffset = (region.Offset + alignment - 1) & ~(alignment - 1);
		uint64_t padding       = alignedOffset - region.Offset;
		uint64_t available     = region.Size - padding;

		if (available >= size)
		{
			Allocation alloc;
			alloc.Offset     = alignedOffset;
			alloc.Size       = size;
			alloc.BlockIndex = i;

			// Shrink / remove the free region.
			uint64_t leftover = available - size;
			if (leftover > 0)
			{
				region.Offset += padding + size;
				region.Size    = leftover;
			}
			else
			{
				// Region fully consumed.
				region = m_FreeRegions.back();
				m_FreeRegions.pop_back();
			}

			// Mark the owning block's usage (find it).
			if (i < m_Blocks.size())
				m_Blocks[i].Used += size;

			return alloc;
		}
	}

	// No existing region fits — allocate a new block.
	uint64_t allocBlockSize = std::max(blockSize, size + alignment);
	MemoryBlock block;
	block.Size      = allocBlockSize;
	block.Used      = size;
	block.Type      = memoryType;
	block.RawHandle = nullptr; // backend fills this in
	m_Blocks.push_back(block);

	uint64_t blockIndex = m_Blocks.size() - 1;
	uint64_t alignedOffset = 0;
	uint64_t leftover      = allocBlockSize - size;

	// Add leftover as a new free region.
	if (leftover > 0)
	{
		FreeRegion r;
		r.Offset = size;
		r.Size   = leftover;
		m_FreeRegions.push_back(r);
	}

	Allocation alloc;
	alloc.Offset     = alignedOffset;
	alloc.Size       = size;
	alloc.BlockIndex = blockIndex;
	return alloc;
}

void RHIMemoryAllocator::Free(const Allocation& alloc)
{
	if (alloc.Size == 0)
		return;

	FreeRegion r;
	r.Offset = alloc.Offset;
	r.Size   = alloc.Size;
	m_FreeRegions.push_back(r);

	// Coalesce adjacent free regions (simple O(n log n) pass).
	std::sort(m_FreeRegions.begin(), m_FreeRegions.end(),
	          [](const FreeRegion& a, const FreeRegion& b) { return a.Offset < b.Offset; });

	for (size_t i = 0; i + 1 < m_FreeRegions.size(); )
	{
		if (m_FreeRegions[i].Offset + m_FreeRegions[i].Size == m_FreeRegions[i + 1].Offset)
		{
			m_FreeRegions[i].Size += m_FreeRegions[i + 1].Size;
			m_FreeRegions[i + 1] = m_FreeRegions.back();
			m_FreeRegions.pop_back();
		}
		else
		{
			++i;
		}
	}

	// Decrement block usage (best-effort).
	if (alloc.BlockIndex < m_Blocks.size() && m_Blocks[alloc.BlockIndex].Used >= alloc.Size)
		m_Blocks[alloc.BlockIndex].Used -= alloc.Size;
}

void RHIMemoryAllocator::Reset()
{
	m_Blocks.clear();
	m_FreeRegions.clear();
}

uint64_t RHIMemoryAllocator::GetTotalAllocated(MemoryType type) const
{
	uint64_t total = 0;
	for (const auto& b : m_Blocks)
		if (b.Type == type) total += b.Size;
	return total;
}

uint64_t RHIMemoryAllocator::GetTotalUsed(MemoryType type) const
{
	uint64_t total = 0;
	for (const auto& b : m_Blocks)
		if (b.Type == type) total += b.Used;
	return total;
}

uint64_t RHIMemoryAllocator::GetBlockCount(MemoryType type) const
{
	uint64_t count = 0;
	for (const auto& b : m_Blocks)
		if (b.Type == type) ++count;
	return count;
}

// =========================================================================
// RHIShaderLibrary
// =========================================================================

RHIShaderLibrary::~RHIShaderLibrary() = default;

Candy::Ref<Candy::RHIShaderModule> RHIShaderLibrary::GetOrCreate(
	uint64_t contentHash,
	Candy::ShaderStage stage,
	std::string_view debugName,
	const std::function<Candy::Ref<Candy::RHIShaderModule>()>& factory)
{
	auto it = m_Cache.find(contentHash);
	if (it != m_Cache.end())
		return it->second.Module;

	auto module = factory();
	if (!module)
		return nullptr;

	Entry entry;
	entry.Module    = module;
	entry.Stage     = stage;
	entry.DebugName = std::string(debugName);
	m_Cache.emplace(contentHash, std::move(entry));
	return module;
}

uint64_t RHIShaderLibrary::HashBytes(const void* data, size_t size)
{
	// FNV-1a over the raw bytes.
	uint64_t hash = 14695981039346656037ULL;
	const auto* bytes = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

uint64_t RHIShaderLibrary::MakeKey(uint64_t contentHash, Candy::ShaderStage stage, std::string_view entryPoint)
{
	// Fold stage and entry point into the content hash so identical source
	// compiled as different stages/entries maps to distinct cache entries.
	uint64_t key = contentHash;
	key ^= static_cast<uint64_t>(stage) + 0x9e3779b97f4a7c15ULL + (key << 6) + (key >> 2);
	uint64_t entryHash = entryPoint.empty() ? 0 : HashBytes(entryPoint.data(), entryPoint.size());
	key ^= entryHash + 0x9e3779b97f4a7c15ULL + (key << 6) + (key >> 2);
	return key;
}

void RHIShaderLibrary::Clear()
{
	m_Cache.clear();
}

} // namespace Candy
