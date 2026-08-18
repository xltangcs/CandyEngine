#pragma once

#include <string>
#include <vector>

namespace Candy {

	// Manages editor docking layout presets (Godot-style).
	//
	// There are two kinds of layout presets:
	//   - User layouts:      VFS://Engine/Saved/LayoutPreset/<name>.ini   (per-machine, saved at runtime)
	//   - Built-in default:  VFS://Engine/Config/DefaultLayout.ini   (tracked by git)
	// Both are stored as ImGui .ini snapshots and are intentionally separate from the
	// runtime auto-saved VFS://Engine/Saved/imgui.ini (which is where the *current*
	// layout lives).
	//
	// A layout preset is the full ImGui ini data (docking tree + window DockIds),
	// obtained via ImGui::SaveIniSettingsToMemory() and applied via
	// ImGui::ClearIniSettings() + ImGui::LoadIniSettingsFromMemory().
	class LayoutPresetManager
	{
	public:
		// Directory for user-saved presets (VFS path).
		static std::string GetUserPresetDirectory();   // "VFS://Engine/Saved/LayoutPreset"
		static const char* GetDefaultPresetName();     // "DefaultLayout"
		static std::string GetPresetPath(const std::string& name);   // VFS://Engine/Saved/LayoutPreset/<name>.ini

		// List all user-saved presets on disk (excluding the built-in default).
		static std::vector<std::string> ListPresets();

		// Persist current editor layout as a user preset at Saved/LayoutPreset/<name>.ini.
		static bool SaveCurrent(const std::string& name);

		// Load a user preset by name and apply it to the current editor context.
		static bool LoadPreset(const std::string& name);

		// Delete a user preset by name. Returns false on failure.
		static bool DeletePreset(const std::string& name);

		// Apply the built-in default layout. Built programmatically via DockBuilder so
		// it adapts to the current window size (ini snapshots use absolute pixels and
		// break when the window size differs from when the layout was captured).
		static void ApplyDefault();
	};

}
