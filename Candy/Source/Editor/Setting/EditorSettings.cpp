#include "CandyPCH.h"
#include "EditorSettings.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>

namespace Candy {

	EditorSettings& EditorSettings::Get()
	{
		static EditorSettings instance;
		return instance;
	}

	void EditorSettings::Save()
	{
		std::filesystem::create_directories("Config");
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "FontSize" << YAML::Value << m_FontSize;
		out << YAML::Key << "FontPath" << YAML::Value << m_FontPath;
		out << YAML::Key << "ShowPhysicsColliders" << YAML::Value << m_ShowPhysicsColliders;
		out << YAML::Key << "ThumbnailSize" << YAML::Value << m_ThumbnailSize;
		out << YAML::Key << "ThumbnailPadding" << YAML::Value << m_ThumbnailPadding;
		out << YAML::Key << "ContentBrowserTreeWidth" << YAML::Value << m_ContentBrowserTreeWidth;
		out << YAML::Key << "ColumnWidth" << YAML::Value << m_ColumnWidth;
		out << YAML::Key << "AutoOpenLastProject" << YAML::Value << m_AutoOpenLastProject;
		out << YAML::EndMap << YAML::EndMap;
		std::ofstream("Config/EditorSettings.candy") << out.c_str();
	}

	void EditorSettings::Load()
	{
		auto path = std::filesystem::path("Config/EditorSettings.candy");
		if (!std::filesystem::exists(path)) return;
		auto doc = YAML::LoadFile(path.string());
		auto s = doc["EditorSettings"];
		if (!s) return;
		if (s["FontSize"])
		{
			m_FontSize = s["FontSize"].as<float>();
		}
		if (s["FontPath"])
		{
			std::string fontPath = s["FontPath"].as<std::string>();
			if (std::filesystem::exists(fontPath))
			{
				m_FontPath = fontPath;
			}
		}
		if (s["ShowPhysicsColliders"]) m_ShowPhysicsColliders = s["ShowPhysicsColliders"].as<bool>();
		if (s["ThumbnailSize"]) m_ThumbnailSize = s["ThumbnailSize"].as<float>();
		if (s["ThumbnailPadding"]) m_ThumbnailPadding = s["ThumbnailPadding"].as<float>();
		if (s["ContentBrowserTreeWidth"]) m_ContentBrowserTreeWidth = s["ContentBrowserTreeWidth"].as<float>();
		if (s["ColumnWidth"]) m_ColumnWidth = s["ColumnWidth"].as<float>();
		if (s["AutoOpenLastProject"]) m_AutoOpenLastProject = s["AutoOpenLastProject"].as<bool>();
	}

}
