#pragma once

#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHIShader.h"
#include "Runtime/Core/Base.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace Candy::IR {

	// =========================================================================
	// IRShaderLibrary — content-hash dedup cache for shader modules
	//
	// Backend-agnostic by design: modules may originate from HLSL source
	// (D3D12), GLSL source (OpenGL), or SPIR-V bytecode (Vulkan) — this library
	// never inspects the payload. Callers supply a content key (hash of
	// source/bytecode + stage + entry point) and a factory invoked only on
	// cache miss, so the same shader is never compiled/created twice.
	//
	// NOTE: CandyEngine intentionally does NOT use SPIR-V as a cross-backend
	// shader IR (per-API source files are the strategy; Slang is the future
	// migration candidate). See AGENTS.md.
	// =========================================================================
	class IRShaderLibrary
	{
	public:
		IRShaderLibrary() = default;
		~IRShaderLibrary();

		/// Returns the cached module for `contentHash`, or invokes `factory`
		/// once to create, cache, and return it.
		Candy::Ref<Candy::RHIShaderModule> GetOrCreate(
			uint64_t                                contentHash,
			Candy::ShaderStage                      stage,
			std::string_view                        debugName,
			const std::function<Candy::Ref<Candy::RHIShaderModule>()>& factory);

		/// FNV-1a over raw bytes — the content part of the cache key.
		static uint64_t HashBytes(const void* data, size_t size);

		/// Combine content hash with stage/entry so the same file compiled as
		/// different stages/entries yields distinct cache entries.
		static uint64_t MakeKey(uint64_t contentHash, Candy::ShaderStage stage, std::string_view entryPoint);

		void Clear();
		[[nodiscard]] size_t GetShaderCount() const { return m_Cache.size(); }

	private:
		struct Entry
		{
			Candy::Ref<Candy::RHIShaderModule> Module;
			Candy::ShaderStage                 Stage = Candy::ShaderStage::None;
			std::string                        DebugName;
		};

		std::unordered_map<uint64_t, Entry> m_Cache; ///< key = MakeKey(content, stage, entry)
	};

} // namespace Candy::IR
