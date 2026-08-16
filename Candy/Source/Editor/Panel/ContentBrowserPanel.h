#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "Runtime/Renderer/Texture.h"
#include "Runtime/Core/VfsPath.h"

namespace Candy {

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();
		void OnImGuiRender();
		// Currently-selected asset VFS path (e.g. "VFS://Game/Materials/Default.mat");
		// empty when nothing is selected. Updated by single-click in the grid.
		const std::string& GetSelectedAsset() const { return m_SelectedAsset; }
	private:
		enum class Domain { Game, Engine };

		Domain m_CurrentDomain = Domain::Game;
		std::filesystem::path m_CurrentRelDir;   // relative to domain root, e.g. "Textures/Sub"
		std::string m_SelectedTreePath;          // "Game/Textures" or "Engine/Icons" for highlight
		std::string m_SearchFilter;
		float m_TreePaneWidth = 240.0f;

		// Selection state for the asset grid (clicked file/folder).
		// Stored as VFS path (e.g. "VFS://Game/Materials/Default.mat") so it
		// can be shared with the Properties panel via the same format.
		std::string m_SelectedAsset;

		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;

		void DrawBreadcrumb();
		void DrawDirectoryTree();
		void DrawTreeRecursive(const std::filesystem::path& dir, Domain domain);
		void DrawContentGrid();
		std::filesystem::path ResolveDiskPath(Domain d, const std::filesystem::path& rel);

		// Asset creation helpers (right-click context menu).
		void CreateNewFolder();
		void CreateNewMaterial();

		// Returns a unique non-colliding name under baseDir; if the candidate
		// name already exists, appends "_1", "_2", ... until it doesn't.
		// If `extension` is non-empty (e.g. ".mat"), it's appended to baseName.
		static std::filesystem::path MakeUniquePath(
			const std::filesystem::path& baseDir,
			const std::string& baseName,
			const std::string& extension);

		// Returns the appropriate icon for a file/folder entry.
		Ref<Texture2D> GetIconForFile(const std::filesystem::path& filename, bool isDirectory) const;

		static std::string DomainLabel(Domain d) { return d == Domain::Game ? "Game" : "Engine"; }
		static VfsPath::Domain ToVfsDomain(Domain d) { return d == Domain::Game ? VfsPath::Domain::Game : VfsPath::Domain::Engine; }
	};

}
