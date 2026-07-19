#include "CandyPCH.h"
#include "EditorSettings.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>

#include "Runtime/Core/Application.h"

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
		out << YAML::Key << "FontSize" << YAML::Value << Application::Get().GetFontSize();
		out << YAML::Key << "FontPath" << YAML::Value << Application::Get().GetFontPath();
		out << YAML::Key << "ThumbnailSize" << YAML::Value << m_ThumbnailSize;
		out << YAML::Key << "ThumbnailPadding" << YAML::Value << m_ThumbnailPadding;
		out << YAML::Key << "ContentBrowserTreeWidth" << YAML::Value << m_ContentBrowserTreeWidth;
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
			float fontSize = s["FontSize"].as<float>();
			Application::Get().SetFontSize(fontSize);
		}
		if (s["FontPath"])
		{
			std::string fontPath = s["FontPath"].as<std::string>();
			if (std::filesystem::exists(fontPath))
			{
				Application::Get().SetFontPath(fontPath);
			}
		}
		if (s["ThumbnailSize"]) m_ThumbnailSize = s["ThumbnailSize"].as<float>();
		if (s["ThumbnailPadding"]) m_ThumbnailPadding = s["ThumbnailPadding"].as<float>();
		if (s["ContentBrowserTreeWidth"]) m_ContentBrowserTreeWidth = s["ContentBrowserTreeWidth"].as<float>();
		if (s["AutoOpenLastProject"]) m_AutoOpenLastProject = s["AutoOpenLastProject"].as<bool>();
	}

}
