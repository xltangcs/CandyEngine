#include "CandyPCH.h"
#include "LayoutPresetManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Candy {

	const char* LayoutPresetManager::GetUserPresetDirectory()
	{
		return "Saved/LayoutPreset";
	}

	const char* LayoutPresetManager::GetDefaultPresetName()
	{
		return "DefaultLayout";
	}

	std::string LayoutPresetManager::GetPresetPath(const std::string& name)
	{
		return std::string(GetUserPresetDirectory()) + "/" + name + ".ini";
	}

	std::vector<std::string> LayoutPresetManager::ListPresets()
	{
		std::vector<std::string> presets;
		std::error_code ec;
		if (!std::filesystem::exists(GetUserPresetDirectory(), ec))
			return presets;

		for (const auto& entry : std::filesystem::directory_iterator(GetUserPresetDirectory(), ec))
		{
			if (!entry.is_regular_file(ec))
				continue;
			std::string filename = entry.path().filename().string();
			if (entry.path().extension() != ".ini")
				continue;

			// Strip the ".ini" extension.
			std::string name = filename.substr(0, filename.size() - 4);
			if (name.empty())
				continue;
			presets.push_back(name);
		}
		return presets;
	}

	bool LayoutPresetManager::SaveCurrent(const std::string& name)
	{
		if (name.empty())
			return false;

		size_t size = 0;
		const char* data = ImGui::SaveIniSettingsToMemory(&size);
		if (!data)
			return false;

		std::error_code ec;
		std::filesystem::create_directories(GetUserPresetDirectory(), ec);

		std::ofstream out(GetPresetPath(name), std::ios::out | std::ios::binary);
		if (!out.is_open())
			return false;
		out.write(data, (std::streamsize)size);
		out.close();
		return true;
	}

	// Read an .ini snapshot from disk and apply it to the current ImGui context.
	// Returns false if the file is missing or could not be read.
	static bool ApplyIniFromFile(const std::string& path)
	{
		if (!std::filesystem::exists(path))
			return false;

		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open())
			return false;

		std::stringstream ss;
		ss << in.rdbuf();
		std::string data = ss.str();
		if (data.empty())
			return false;

		// Clear the current window/layout state, then apply the preset's ini data.
		// This ensures we don't retain stale window positions or docking from the
		// currently cached settings. Works because EditorLayer's ImGui context at
		// this point is the editor context.
		ImGui::ClearIniSettings();
		ImGui::LoadIniSettingsFromMemory(data.c_str(), data.size());
		return true;
	}

	bool LayoutPresetManager::LoadPreset(const std::string& name)
	{
		return ApplyIniFromFile(GetPresetPath(name));
	}

	bool LayoutPresetManager::DeletePreset(const std::string& name)
	{
		if (name.empty())
			return false;

		std::string path = GetPresetPath(name);
		std::error_code ec;
		if (!std::filesystem::exists(path, ec))
			return false;
		return std::filesystem::remove(path, ec);
	}

	// Programmatically build the default layout when no DefaultLayout.ini is present.
	// Matches the layout captured in the bundled DefaultLayout.ini:
	//   left: Scene Hierarchy | Viewport (split X)
	//   below: Content Browser (split Y)
	//   right: Properties + Stats (split X against the left column)
	static void BuildDefaultLayoutDockBuilder()
	{
		ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");

		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

		// Split the dockspace: right column (Properties/Stats) vs the rest.
		ImGuiID dockRight, dockMain;
		ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.27f, &dockRight, &dockMain);

		// Split the left/main area: top (Hierarchy | Viewport) vs bottom (Content Browser).
		ImGuiID dockTop, dockBottom;
		ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.34f, &dockBottom, &dockTop);

		// Split the top area into Hierarchy (left) and Viewport (center).
		ImGuiID dockHierarchy, dockViewport;
		ImGui::DockBuilderSplitNode(dockTop, ImGuiDir_Left, 0.30f, &dockHierarchy, &dockViewport);

		// Split the right column into Properties (top) and Stats (bottom).
		ImGuiID dockProperties, dockStats;
		ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.50f, &dockStats, &dockProperties);

		ImGui::DockBuilderDockWindow("Scene Hierarchy", dockHierarchy);
		ImGui::DockBuilderDockWindow("Viewport", dockViewport);
		ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
		ImGui::DockBuilderDockWindow("Properties", dockProperties);
		ImGui::DockBuilderDockWindow("Stats", dockStats);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	void LayoutPresetManager::ApplyDefault()
	{
		// Build the default layout programmatically via DockBuilder so it adapts to the
		// current window size. ImGui .ini layout data is stored in absolute pixels, so a
		// preset captured at a different window size won't map correctly; the DockBuilder
		// uses proportional splits based on the current viewport and therefore always fits.
		ImGui::ClearIniSettings();
		BuildDefaultLayoutDockBuilder();
	}

}
