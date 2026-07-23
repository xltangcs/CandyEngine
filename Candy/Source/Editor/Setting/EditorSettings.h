#pragma once

#include <string>

namespace Candy {

	class EditorSettings
	{
	public:
		static EditorSettings& Get();

		void Save();
		void Load();
		
		float m_ThumbnailSize = 256.0f;
		float m_ThumbnailPadding = 16.0f;
		float m_ContentBrowserTreeWidth = 240.0f;
		float m_ColumnWidth = 150.0f;
		bool m_AutoOpenLastProject = false;

		float m_FontSize = 18.0f;
		std::string m_FontPath = "VFS://Engine/Fonts/opensans/OpenSans-Regular.ttf";

		bool m_ShowPhysicsColliders = false;

	private:
		EditorSettings() = default;
	};

}
