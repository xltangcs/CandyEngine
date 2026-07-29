#pragma once

#include "Runtime/RHI/RHIPipelineState.h"
#include "Runtime/Core/Base.h"

#include <unordered_map>
#include <string>
#include <functional>

namespace Candy::IR {

	// =========================================================================
	// IRPipelineCache — caches RHIGraphicsPipeline objects keyed by
	// GraphicsPipelineDesc hash.
	//
	// Pipeline creation (VkPipeline / ID3D12PipelineState) is expensive.
	// This cache ensures identical pipeline descriptions reuse the same PSO.
	// =========================================================================
	class IRPipelineCache
	{
	public:
		IRPipelineCache() = default;
		~IRPipelineCache();

		// ---- Cache operations ----------------------------------------------

		/// Returns a cached pipeline if one exists; nullptr otherwise.
		[[nodiscard]] Candy::Ref<Candy::RHIGraphicsPipeline> Find(const Candy::GraphicsPipelineDesc& desc) const;

		/// Inserts a pipeline into the cache.  Returns false if already present.
		bool Insert(const Candy::GraphicsPipelineDesc& desc, const Candy::Ref<Candy::RHIGraphicsPipeline>& pipeline);

		/// Removes a pipeline entry from the cache.
		void Erase(const Candy::GraphicsPipelineDesc& desc);

		/// Removes all cached pipelines.
		void Clear();

		// ---- Query ---------------------------------------------------------

		[[nodiscard]] size_t GetCount() const { return m_Cache.size(); }
		[[nodiscard]] bool   IsEmpty() const  { return m_Cache.empty(); }

		// ---- Utilities -----------------------------------------------------

		/// Computes a deterministic hash for a pipeline description.
		static size_t HashDesc(const Candy::GraphicsPipelineDesc& desc);

	private:
		std::unordered_map<size_t, Candy::Ref<Candy::RHIGraphicsPipeline>> m_Cache;
	};

} // namespace Candy::IR
