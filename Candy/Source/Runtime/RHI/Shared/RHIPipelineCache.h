#pragma once

#include "Runtime/RHI/RHIPipelineState.h"
#include "Runtime/Core/Base.h"

#include <unordered_map>
#include <string>
#include <functional>

namespace Candy {

	class RHIShaderModule;

	// =========================================================================
	// RHIPipelineCache — caches RHIGraphicsPipeline objects keyed by
	// GraphicsPipelineDesc hash + vertex/fragment shader content hashes.
	//
	// Pipeline creation (VkPipeline / ID3D12PipelineState) is expensive.
	// This cache ensures identical pipeline descriptions + shaders reuse the
	// same PSO. Shaders are part of the key: the same desc with a different
	// shader (e.g. PBR.hlsl vs Sprite.hlsl) must produce a different PSO.
	// =========================================================================
	class RHIPipelineCache
	{
	public:
		RHIPipelineCache() = default;
		~RHIPipelineCache();

		// ---- Cache operations ----------------------------------------------

		/// Returns a cached pipeline if one exists; nullptr otherwise.
		[[nodiscard]] Candy::Ref<Candy::RHIGraphicsPipeline> Find(
			const Candy::GraphicsPipelineDesc& desc,
			const Candy::Ref<Candy::RHIShaderModule>& vs,
			const Candy::Ref<Candy::RHIShaderModule>& fs) const;

		/// Inserts a pipeline into the cache.  Returns false if already present.
		bool Insert(
			const Candy::GraphicsPipelineDesc& desc,
			const Candy::Ref<Candy::RHIShaderModule>& vs,
			const Candy::Ref<Candy::RHIShaderModule>& fs,
			const Candy::Ref<Candy::RHIGraphicsPipeline>& pipeline);

		/// Removes a pipeline entry from the cache.
		void Erase(const Candy::GraphicsPipelineDesc& desc,
		           const Candy::Ref<Candy::RHIShaderModule>& vs,
		           const Candy::Ref<Candy::RHIShaderModule>& fs);

		/// Removes all cached pipelines.
		void Clear();

		// ---- Query ---------------------------------------------------------

		[[nodiscard]] size_t GetCount() const { return m_Cache.size(); }
		[[nodiscard]] bool   IsEmpty() const  { return m_Cache.empty(); }

		// ---- Utilities -----------------------------------------------------

		/// Computes a deterministic hash for a pipeline description + shaders.
		static size_t HashDesc(
			const Candy::GraphicsPipelineDesc& desc,
			const Candy::Ref<Candy::RHIShaderModule>& vs,
			const Candy::Ref<Candy::RHIShaderModule>& fs);

	private:
		std::unordered_map<size_t, Candy::Ref<Candy::RHIGraphicsPipeline>> m_Cache;
	};

} // namespace Candy
