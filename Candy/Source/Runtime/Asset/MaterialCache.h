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
	//   * Touch(path)   — bumps the revision counter; the renderer compares
	//                     revisions to detect "material changed" and rebuilds
	//                     GPU state (PSO / uploaded constants) accordingly.
	// =========================================================================
	class MaterialCache
	{
	public:
		static MaterialCache& Get();

		// Loads (and caches) the material at the given VFS path. Returns nullptr
		// if the path is empty or the file cannot be loaded.
		Ref<Material> Load(const std::string& vfsPath);

		// Removes the cached entry (if any) for the given path and bumps the
		// revision (the file on disk may have changed).
		void Reload(const std::string& vfsPath);

		// Removes all cached entries and revisions.
		void Clear();

		// Marks the material at vfsPath as modified (call after saving to disk).
		void Touch(const std::string& vfsPath);

		// Revision of the material at vfsPath (0 if never loaded or touched).
		uint64_t GetRevision(const std::string& vfsPath) const;

	private:
		MaterialCache() = default;

		std::unordered_map<std::string, Ref<Material>> m_Materials;
		std::unordered_map<std::string, uint64_t>       m_Revisions;
	};

}
