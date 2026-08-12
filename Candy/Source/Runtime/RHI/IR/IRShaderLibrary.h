#pragma once

#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHIShader.h"
#include "Runtime/Core/Base.h"

#include <unordered_map>
#include <string>
#include <span>
#include <cstddef>

namespace Candy {
	class RHIDevice;
} // namespace Candy

namespace Candy::IR {

	// =========================================================================
	// IRShaderLibrary — manages compiled SPIR-V shader modules
	//
	// Provides:
	//  - Deduplication: identical bytecode → same shader module handle
	//  - Caching: avoid redundant driver calls for CreateShaderModule()
	//  - Future: SPIR-V → HLSL / MSL cross-compilation (via SPIRV-Cross)
	// =========================================================================
	class IRShaderLibrary
	{
	public:
		struct ShaderEntry
		{
			Candy::Ref<Candy::RHIShaderModule> Module;
			Candy::ShaderStage                 Stage     = Candy::ShaderStage::None;
			std::string                        DebugName;
			uint64_t                           Hash      = 0;
		};

		IRShaderLibrary() = default;
		~IRShaderLibrary();

		// ---- Load from SPIR-V ----------------------------------------------

		/// Creates (or retrieves a cached) shader module from SPIR-V bytecode.
		/// 'device' is used to create the underlying RHIShaderModule.
		[[nodiscard]] Candy::RHIHandle LoadShader(
			Candy::RHIDevice&              device,
			std::span<const uint32_t>      spirvBytecode,
			Candy::ShaderStage             stage,
			std::string_view               debugName = "");

		/// Loads SPIR-V from raw 8-bit bytes (e.g. from file).
		[[nodiscard]] Candy::RHIHandle LoadShader(
			Candy::RHIDevice&              device,
			std::span<const std::byte>     spirvRaw,
			Candy::ShaderStage             stage,
			std::string_view               debugName = "");

		// ---- Lookup --------------------------------------------------------

		[[nodiscard]] Candy::Ref<Candy::RHIShaderModule> GetShader(Candy::RHIHandle handle) const;
		[[nodiscard]] Candy::ShaderStage                 GetStage(Candy::RHIHandle handle) const;

		void ReleaseShader(Candy::RHIHandle handle);

		// ---- Cache ---------------------------------------------------------

		void Clear();
		[[nodiscard]] size_t GetShaderCount() const { return m_Shaders.size(); }

		// ---- Utilities -----------------------------------------------------

		/// Computes a content hash from SPIR-V bytecode.
		static uint64_t HashBytecode(std::span<const uint32_t> spirv);

	private:
		std::unordered_map<Candy::RHIHandle, ShaderEntry> m_Shaders;
		std::unordered_map<uint64_t, Candy::RHIHandle>     m_HashToHandle; ///< dedup lookup
		uint32_t m_NextHandle = 1;
	};

} // namespace Candy::IR
