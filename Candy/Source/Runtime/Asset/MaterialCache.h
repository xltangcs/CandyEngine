#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/Material.h"

#include <string>
#include <unordered_map>

namespace Candy {

	// =========================================================================
	// MaterialCache — global registry of loaded material assets.
	//
	// Guarantees that the same .mat VFS path resolves to a single shared
	// Ref<Material>. This is what makes a .mat "asset" reusable across multiple
	// StaticMeshComponents: editing it once affects every user.
	//
	//   * Load(path)    — returns the cached material, or loads + caches on miss.
	//   * Reload(path)  — drops the cached instance so the next Load re-reads the
	//                     file from disk (used after editing in the inspector).
	// =========================================================================
	class MaterialCache
	{
	public:
		static MaterialCache& Get();

		// Loads (and caches) the material at the given VFS path. Returns nullptr
		// if the path is empty or the file cannot be loaded.
		Ref<Material> Load(const std::string& vfsPath);

		// Removes the cached entry (if any) for the given path.
		void Reload(const std::string& vfsPath);

		// Removes all cached entries.
		void Clear();

	private:
		MaterialCache() = default;

		std::unordered_map<std::string, Ref<Material>> m_Materials;
	};

}
