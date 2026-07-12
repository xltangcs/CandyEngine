#pragma once

#include <string>

namespace Candy {

	class CandyEditorState
	{
	public:
		static CandyEditorState& Get();

		void Save();
		void Load();

		std::string LastScenePath;
		int WindowWidth = 1280;
		int WindowHeight = 720;
		bool WindowMaximized = false;
		bool ShowProjectSettings = false;
		bool ShowEditorSettings = false;

	private:
		CandyEditorState() = default;
	};

}
