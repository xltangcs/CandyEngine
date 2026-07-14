#pragma once

#include <string>

namespace Candy {

	class EditorSettings
	{
	public:
		static EditorSettings& Get();

		void OnImGuiRender();
		void Save();
		void Load();

		float m_FontSize = 18.0f;
		std::string m_FontPath = "Assets/fonts/opensans/OpenSans-Regular.ttf";
		float m_ThumbnailSize = 256.0f;
		float m_ThumbnailPadding = 16.0f;
		bool m_AutoOpenLastProject = false;

	private:
		EditorSettings() = default;
	};

}
