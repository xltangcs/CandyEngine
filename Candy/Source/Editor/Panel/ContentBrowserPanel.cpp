#include "CandyPCH.h"
#include "ContentBrowserPanel.h"
#include "Setting/EditorSettings.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include "Runtime/Core/Application.h"
#include "Runtime/Project/ProjectUtils.h"

#include <algorithm>
#include <cctype>

namespace Candy {

	ContentBrowserPanel::ContentBrowserPanel()
	{
		m_DirectoryIcon = Texture2D::Create("VFS://Engine/Icons/ContentBrowser/DirectoryIcon.png");
		m_FileIcon = Texture2D::Create("VFS://Engine/Icons/ContentBrowser/FileIcon.png");
		m_TreePaneWidth = EditorSettings::Get().m_ContentBrowserTreeWidth;
		if (m_TreePaneWidth < 160.0f) m_TreePaneWidth = 240.0f;
	}

	std::filesystem::path ContentBrowserPanel::ResolveDiskPath(Domain d, const std::filesystem::path& rel)
	{
		auto root = (d == Domain::Game) ? ProjectUtils::GetProjectContentPath()
		                                : ProjectUtils::GetEngineContentPath();
		return root / rel;
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Content Browser");

		// Top: breadcrumb + search
		DrawBreadcrumb();

		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##Search", "Search...", &m_SearchFilter);
		ImGui::PopItemWidth();

		ImGui::Separator();

		float h = ImGui::GetContentRegionAvail().y;

		// Left: directory tree
		ImGui::BeginChild("##TreePane", ImVec2(m_TreePaneWidth, h), true);
		DrawDirectoryTree();
		ImGui::EndChild();

		ImGui::SameLine();

		// Splitter
		ImGui::InvisibleButton("##Splitter", ImVec2(4.0f, h));
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (ImGui::IsItemActive())
		{
			m_TreePaneWidth += ImGui::GetIO().MouseDelta.x;
			m_TreePaneWidth = std::max(160.0f, std::min(m_TreePaneWidth, 600.0f));
		}
		if (ImGui::IsItemDeactivated())
		{
			EditorSettings::Get().m_ContentBrowserTreeWidth = m_TreePaneWidth;
			EditorSettings::Get().Save();
		}

		ImGui::SameLine();

		// Right: content grid
		ImGui::BeginChild("##ContentPane", ImVec2(0, h), true);
		DrawContentGrid();
		ImGui::EndChild();

		ImGui::End();
	}

	void ContentBrowserPanel::DrawBreadcrumb()
	{
		const std::string domainLabel = DomainLabel(m_CurrentDomain);

		if (ImGui::SmallButton(domainLabel.c_str()))
		{
			m_CurrentRelDir.clear();
			m_SelectedTreePath = domainLabel;
		}

		std::filesystem::path accumulated;
		for (auto& segment : m_CurrentRelDir)
		{
			accumulated /= segment;
			ImGui::SameLine();
			ImGui::TextUnformatted("/");
			ImGui::SameLine();
			if (ImGui::SmallButton(segment.string().c_str()))
			{
				m_CurrentRelDir = accumulated;
				m_SelectedTreePath = domainLabel + "/" + accumulated.generic_string();
			}
		}
	}

	void ContentBrowserPanel::DrawDirectoryTree()
	{
		// Game root first
		ImGuiTreeNodeFlags gameRootFlags = ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_OpenOnDoubleClick
			| ImGuiTreeNodeFlags_DefaultOpen;
		if (m_CurrentDomain == Domain::Game && m_CurrentRelDir.empty())
			gameRootFlags |= ImGuiTreeNodeFlags_Selected;

		bool gameOpen = ImGui::TreeNodeEx("Game", gameRootFlags);
		if (ImGui::IsItemClicked())
		{
			m_CurrentDomain = Domain::Game;
			m_CurrentRelDir.clear();
			m_SelectedTreePath = "Game";
		}
		if (gameOpen)
		{
			DrawTreeRecursive(std::filesystem::path(), Domain::Game);
			ImGui::TreePop();
		}

		// Engine root second
		ImGuiTreeNodeFlags engineRootFlags = ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_OpenOnDoubleClick
			| ImGuiTreeNodeFlags_DefaultOpen;
		if (m_CurrentDomain == Domain::Engine && m_CurrentRelDir.empty())
			engineRootFlags |= ImGuiTreeNodeFlags_Selected;

		bool engineOpen = ImGui::TreeNodeEx("Engine", engineRootFlags);
		if (ImGui::IsItemClicked())
		{
			m_CurrentDomain = Domain::Engine;
			m_CurrentRelDir.clear();
			m_SelectedTreePath = "Engine";
		}
		if (engineOpen)
		{
			DrawTreeRecursive(std::filesystem::path(), Domain::Engine);
			ImGui::TreePop();
		}
	}

	void ContentBrowserPanel::DrawTreeRecursive(const std::filesystem::path& dir, Domain domain)
	{
		auto diskDir = ResolveDiskPath(domain, dir);
		if (!std::filesystem::exists(diskDir)) return;

		// Collect and sort subdirectories (files are shown in the content grid, not the tree)
		std::vector<std::filesystem::path> subdirs;
		for (auto& entry : std::filesystem::directory_iterator(diskDir))
		{
			if (entry.is_directory())
				subdirs.push_back(entry.path());
		}
		std::sort(subdirs.begin(), subdirs.end());

		const std::string domainLabel = DomainLabel(domain);

		for (auto& subdir : subdirs)
		{
			auto name = subdir.filename().string();
			auto fullTreePath = dir / subdir.filename();

			// Check if this subdir has its own subdirectories (leaf detection)
			bool hasSubDirs = false;
			{
				std::error_code ec;
				for (auto& e : std::filesystem::directory_iterator(subdir, ec))
				{
					if (e.is_directory()) { hasSubDirs = true; break; }
				}
			}

			std::string selectionKey = domainLabel + "/" + fullTreePath.generic_string();
			bool isSelected = (m_SelectedTreePath == selectionKey);

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_OpenOnArrow
				| ImGuiTreeNodeFlags_OpenOnDoubleClick;
			if (!hasSubDirs)
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (isSelected)
				flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::PushID(selectionKey.c_str());
			bool opened = ImGui::TreeNodeEx("node", flags, "%s", name.c_str());
			ImGui::PopID();

			if (ImGui::IsItemClicked())
			{
				m_CurrentDomain = domain;
				m_CurrentRelDir = fullTreePath;
				m_SelectedTreePath = selectionKey;
			}

			if (opened && hasSubDirs)
			{
				DrawTreeRecursive(fullTreePath, domain);
				ImGui::TreePop();
			}
		}
	}

	void ContentBrowserPanel::DrawContentGrid()
	{
		auto& editorSettings = EditorSettings::Get();
		float thumbnailSize = editorSettings.m_ThumbnailSize;
		float padding = editorSettings.m_ThumbnailPadding;
		float cellSize = thumbnailSize + padding;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = static_cast<int>(panelWidth / cellSize);
		columnCount = std::max(1, columnCount);

		ImGui::Columns(columnCount, 0, false);

		auto domainRoot = ResolveDiskPath(m_CurrentDomain, std::filesystem::path());
		auto currentDir = ResolveDiskPath(m_CurrentDomain, m_CurrentRelDir);

		if (!std::filesystem::exists(currentDir))
		{
			ImGui::Columns(1);
			return;
		}

		// Build lowercase search filter
		std::string lowerFilter = m_SearchFilter;
		std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
		               [](unsigned char c) { return std::tolower(c); });

		for (auto& directoryEntry : std::filesystem::directory_iterator(currentDir))
		{
			const auto& path = directoryEntry.path();
			auto relativeToDomain = std::filesystem::relative(path, domainRoot);
			std::string filenameString = relativeToDomain.filename().string();

			// Apply search filter (case-insensitive substring on filename)
			if (!lowerFilter.empty())
			{
				std::string lowerName = filenameString;
				std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
				               [](unsigned char c) { return std::tolower(c); });
				if (lowerName.find(lowerFilter) == std::string::npos)
					continue;
			}

			// Build VFS:// path for drag-drop payload
			VfsPath vp(ToVfsDomain(m_CurrentDomain), relativeToDomain.generic_string());
			std::string vfsPathStr = vp.ToString();

			ImGui::PushID(filenameString.c_str());
			Ref<Texture2D> icon = directoryEntry.is_directory() ? m_DirectoryIcon : m_FileIcon;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::ImageButton(filenameString.c_str(), (ImTextureID)icon->GetRendererID(), { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 });

			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", vfsPathStr.c_str(), vfsPathStr.size() + 1);
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (directoryEntry.is_directory())
				{
					m_CurrentRelDir /= path.filename();
					m_SelectedTreePath = DomainLabel(m_CurrentDomain) + "/" + m_CurrentRelDir.generic_string();
				}
			}

			ImGui::TextWrapped("%s", filenameString.c_str());
			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);
	}

}
