#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/ShaderReflector.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Candy {

	// =========================================================================
	// ShaderCache --- global registry of reflected shader parameter metadata.
	//
	// Mirrors MaterialCache: the same VFS shader path resolves to a single
	// parameter list. Entries are keyed by a hash of the source text, so an
	// edited .hlsl / .glsl file re-reflects automatically on the next query.
	// =========================================================================
	class ShaderCache
	{
	public:
		static ShaderCache& Get();

		/// Returns the reflected parameter list for the shader at `vfsPath`, or
		/// nullptr when the file cannot be read or contains no @param markers.
		const std::vector<ShaderParameter>* GetParameters(const std::string& vfsPath);

		/// Drops the cached entry so the next GetParameters re-reads the file.
		void Invalidate(const std::string& vfsPath);

		/// Removes all cached entries.
		void Clear();

	private:
		ShaderCache() = default;

		struct Entry
		{
			uint64_t SourceHash = 0;
			std::vector<ShaderParameter> Parameters;
		};

		std::unordered_map<std::string, Entry> m_Entries;
	};

} // namespace Candy
