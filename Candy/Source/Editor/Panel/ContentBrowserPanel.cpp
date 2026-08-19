#include "CandyPCH.h"
#include "ContentBrowserPanel.h"
#include "EditorSelection.h"
#include "Setting/EditorSettings.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include "Runtime/Core/Application.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Asset/Material.h"

#include <algorithm>
#include <cctype>

namespace Candy {

	namespace
	{
		// Lowercase a string (ASCII, for extension matching).
		std::string ToLower(const std::string& s)
		{
			std::string out = s;
			std::transform(out.begin(), out.end(), out.begin(),
			               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		bool Contains(const std::vector<std::string>& list, const std::string& value)
		{
			return std::find(list.begin(), list.end(), value) != list.end();
		}

		// Returns false if `name` is a hidden file/folder that should be skipped.
		// Hidden rules come from the (global) EditorSettings, filled in the
		// Editor Settings panel.
		//   * Folders: hidden by full name only (exact, case-sensitive).
		//   * Files: hidden when their (lowercased) name ends with one of the
		//     configured extensions (case-insensitive suffix match, no auto-dot).
		//     The user controls the rule; e.g. ".png" only matches a trailing
		//     ".png" while "png" also matches names ending in "png".
		bool ShouldDisplayEntry(const std::string& name, bool isDirectory,
		                        const std::vector<std::string>& hiddenFolderNames,
		                        const std::vector<std::string>& hiddenExtensions)
		{
			// Folders are hidden by full name only.
			if (isDirectory)
				return !Contains(hiddenFolderNames, name);

			// Files are hidden by case-insensitive suffix match against their name.
			std::string lower = ToLower(name);
			for (const auto& e : hiddenExtensions)
			{
				if (!e.empty() && lower.ends_with(ToLower(e)))
					return false;
			}
			return true;
		}
	}

	ContentBrowserPanel::ContentBrowserPanel()
	{
		// Editor chrome textures are displayed raw by ImGui (no sRGB decode).
		m_DirectoryIcon = Texture2D::Create("VFS://Engine/Content/Icons/ContentBrowser/DirectoryIcon.png", false);
		m_FileIcon = Texture2D::Create("VFS://Engine/Content/Icons/ContentBrowser/FileIcon.png", false);
		m_TreePaneWidth = EditorSettings::Get().m_ContentBrowserTreeWidth;
		if (m_TreePaneWidth < 160.0f) m_TreePaneWidth = 240.0f;
	}

	std::filesystem::path ContentBrowserPanel::ResolveDiskPath(Domain d, const std::filesystem::path& rel)
	{
		// Engine root lives at ../Candy relative to the editor CWD (resources under Content/)
		static const std::filesystem::path kEngineRoot = std::filesystem::path("..") / "Candy";

		auto root = (d == Domain::Game)
			? Application::Get().GetProject()->GetProjectDirectory()
			: kEngineRoot;
		return root / rel;
	}

	Ref<Texture2D> ContentBrowserPanel::GetIconForFile(const std::filesystem::path& filename, bool isDirectory) const
	{
		if (isDirectory)
			return m_DirectoryIcon;
		// Future: per-extension icons (e.g. .mat -> MaterialIcon.png). For now
		// all files share the generic file icon.
		return m_FileIcon;
	}

	std::filesystem::path ContentBrowserPanel::MakeUniquePath(
		const std::filesystem::path& baseDir,
		const std::string& baseName,
		const std::string& extension)
	{
		std::filesystem::path candidate = baseDir / (baseName + extension);
		if (!std::filesystem::exists(candidate))
			return candidate;

		for (int i = 1; i < 10000; ++i)
		{
			candidate = baseDir / (baseName + "_" + std::to_string(i) + extension);
			if (!std::filesystem::exists(candidate))
				return candidate;
		}
		// Fallback (extremely unlikely to reach).
		return baseDir / (baseName + "_overflow" + extension);
	}

	void ContentBrowserPanel::CreateNewFolder()
	{
		auto currentDir = ResolveDiskPath(m_CurrentDomain, m_CurrentRelDir);
		auto target = MakeUniquePath(currentDir, "NewFolder", "");
		std::error_code ec;
		if (!std::filesystem::create_directory(target, ec))
		{
			CANDY_CORE_ERROR("ContentBrowser: failed to create folder '{}': {}",
				target.string(), ec.message());
		}
	}

	void ContentBrowserPanel::CreateNewMaterial()
	{
		auto currentDir = ResolveDiskPath(m_CurrentDomain, m_CurrentRelDir);
		auto target = MakeUniquePath(currentDir, "NewMaterial", ".mat");

		// Build VFS path for the new file and write a default Material.
		std::string relPath = std::filesystem::relative(target,
			ResolveDiskPath(m_CurrentDomain, std::filesystem::path())).generic_string();
		VfsPath vp(ToVfsDomain(m_CurrentDomain), relPath);
		std::string vfsPath = vp.ToString();

		Ref<Material> mat = CreateRef<Material>();
		mat->Name = target.stem().string();
		if (!mat->Serialize(vfsPath))
		{
			CANDY_CORE_ERROR("ContentBrowser: failed to write new material at '{}'", vfsPath);
		}
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

		auto& editorSettings = EditorSettings::Get();

		// Collect and sort subdirectories (files are shown in the content grid, not the tree)
		std::vector<std::filesystem::path> subdirs;
		for (auto& entry : std::filesystem::directory_iterator(diskDir))
		{
			if (entry.is_directory())
			{
				auto name = entry.path().filename().string();
				if (ShouldDisplayEntry(name, true, editorSettings.m_HiddenFolderNames, editorSettings.m_HiddenExtensions))
					subdirs.push_back(entry.path());
			}
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
			// Even if the dir vanished, still allow right-click on the grid for
			// future folder creation in the (now restored) parent.
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

			// Skip hidden extensions / folder names
			if (!ShouldDisplayEntry(filenameString, directoryEntry.is_directory(),
			                        editorSettings.m_HiddenFolderNames, editorSettings.m_HiddenExtensions))
				continue;

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

			bool isSelected = (m_SelectedAsset == vfsPathStr);

			ImGui::PushID(filenameString.c_str());
			Ref<Texture2D> icon = GetIconForFile(path.filename(), directoryEntry.is_directory());

			// Highlight selected item.
			if (isSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.45f, 0.78f, 0.55f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.50f, 0.85f, 0.75f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.72f, 0.85f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			}
			// UV flip: OpenGL framebuffers are bottom-up (V=0 is bottom); D3D12/Vulkan
			// are top-down (V=0 is top), so the flip would show icons upside down.
			ImVec2 uv0{ 0, 1 }, uv1{ 1, 0 };
			if (Renderer::GetAPI() == RendererAPI::API::D3D12
				|| Renderer::GetAPI() == RendererAPI::API::Vulkan)
			{
				uv0 = ImVec2{ 0, 0 };
				uv1 = ImVec2{ 1, 1 };
			}
			ImGui::ImageButton(filenameString.c_str(), reinterpret_cast<void*>(icon->GetRendererID64()), { thumbnailSize, thumbnailSize }, uv0, uv1);

			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", vfsPathStr.c_str(), vfsPathStr.size() + 1);
				ImGui::EndDragDropSource();
			}

			// Right-click on item: per-file context menu (placeholder for now).
			if (ImGui::BeginPopupContextItem())
			{
				ImGui::TextDisabled("%s", filenameString.c_str());
				ImGui::Separator();
				ImGui::MenuItem("Rename...", nullptr, false, false);
				ImGui::MenuItem("Delete", nullptr, false, false);
				ImGui::EndPopup();
			}

			ImGui::PopStyleColor(isSelected ? 3 : 1);

			// Single-click → select locally (highlight only). Does NOT publish to
			// EditorSelection: the Properties panel is only opened on double-click,
			// matching the UE/Godot habit of "click to select, double-click to edit".
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				m_SelectedAsset = vfsPathStr;
			}

			// Double-click → publish the asset to the Properties panel so it can be
			// inspected/edited there. A double-click on a folder still enters it.
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (directoryEntry.is_directory())
				{
					m_CurrentRelDir /= path.filename();
					m_SelectedTreePath = DomainLabel(m_CurrentDomain) + "/" + m_CurrentRelDir.generic_string();
				}
				else
				{
					EditorSelection::Get().SelectAsset(vfsPathStr);
				}
			}

			ImGui::TextWrapped("%s", filenameString.c_str());
			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);

		// Right-click on empty grid area: New Folder / New Material.
		if (ImGui::BeginPopupContextWindow("##ContentBrowserBlankContext",
		    ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("New Folder"))
			{
				CreateNewFolder();
			}
			if (ImGui::MenuItem("New Material"))
			{
				CreateNewMaterial();
			}
			ImGui::EndPopup();
		}
	}

}
