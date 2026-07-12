#pragma once

#include <string>

namespace Candy {

	class CandyEditorSettings
	{
	public:
		static CandyEditorSettings& Get();

		void OnImGuiRender();
		void Save();
		void Load();

		float FontSize = 18.0f;
		std::string FontPath = "Assets/fonts/opensans/OpenSans-Regular.ttf";
		float ThumbnailSize = 256.0f;
		float ThumbnailPadding = 16.0f;

	private:
		CandyEditorSettings() = default;
	};

}
