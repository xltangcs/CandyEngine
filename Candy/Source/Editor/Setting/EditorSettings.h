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
		bool m_AutoOpenLastProject = false;

	private:
		EditorSettings() = default;
	};

}
