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

	private:
		EditorState() = default;
	};

}
