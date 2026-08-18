#pragma once

#include <string>
#include <vector>
#include "Runtime/Project/RecentProjects.h"

namespace Candy {

	/// Persisted to Saved/EditorState.candy — the single file backing both the
	/// editor session state (window geometry, last opened project) and the
	/// recent projects list shown in the Project Manager.
	class EditorState
	{
	public:
		static EditorState& Get();

		void Load();
		/// Captures the current window geometry into the fields and writes the file.
		void Save();
		/// Writes the in-memory state to disk without re-querying window geometry
		/// (used mid-session, e.g. RecentProjects::Add from the Project Manager).
		void WriteFile();

		std::vector<RecentProjectEntry> RecentProjects;
		std::string LastOpenProject;
		int WindowWidth = 1280;
		int WindowHeight = 720;
		bool WindowMaximized = false;
		bool ShowProjectSettings = false;
		bool ShowEditorSettings = false;
		bool ShowSceneSettings = false;

		// Whether the initial default docking layout has already been applied for
		// a fresh project (no Saved/imgui.ini yet). Persisted so we only apply it once.
		bool LayoutPresetApplied = false;

	private:
		EditorState() = default;
		bool m_Loaded = false;
	};

}
