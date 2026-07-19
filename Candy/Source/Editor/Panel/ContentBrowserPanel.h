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
	private:
		enum class Domain { Game, Engine };

		Domain m_CurrentDomain = Domain::Game;
		std::filesystem::path m_CurrentRelDir;   // relative to domain root, e.g. "Textures/Sub"
		std::string m_SelectedTreePath;          // "Game/Textures" or "Engine/Icons" for highlight
		std::string m_SearchFilter;
		float m_TreePaneWidth = 240.0f;

		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;

		void DrawBreadcrumb();
		void DrawDirectoryTree();
		void DrawTreeRecursive(const std::filesystem::path& dir, Domain domain);
		void DrawContentGrid();
		std::filesystem::path ResolveDiskPath(Domain d, const std::filesystem::path& rel);

		static std::string DomainLabel(Domain d) { return d == Domain::Game ? "Game" : "Engine"; }
		static VfsPath::Domain ToVfsDomain(Domain d) { return d == Domain::Game ? VfsPath::Domain::Game : VfsPath::Domain::Engine; }
	};

}
