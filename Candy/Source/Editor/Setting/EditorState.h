#pragma once

#include <string>

namespace Candy {

	class EditorState
	{
	public:
		static EditorState& Get();

		void Save();
		void Load();

		std::string LastScenePath;
		int WindowWidth = 1280;
		int WindowHeight = 720;
		bool WindowMaximized = false;
		bool ShowProjectSettings = false;
		bool ShowEditorSettings = false;

		// Whether the initial default docking layout has already been applied for
		// a fresh project (no Saved/imgui.ini yet). Persisted so we only apply it once.
		bool LayoutPresetApplied = false;

	private:
		EditorState() = default;
	};

}
