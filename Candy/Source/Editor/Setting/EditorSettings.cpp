#include "CandyPCH.h"
#include "EditorSettings.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include "Runtime/Core/FileSystem.h"

namespace Candy {

	static const char* GetFilePath()
	{
		return "VFS://Engine/Config/EditorSettings.candy";
	}

	EditorSettings& EditorSettings::Get()
	{
		static EditorSettings instance;
		return instance;
	}

	void EditorSettings::Save()
	{
		// Drop empty rows so the settings file stays clean.
		auto cleaned = [](const std::vector<std::string>& list)
		{
			std::vector<std::string> out;
			out.reserve(list.size());
			for (const auto& v : list)
			{
				if (!v.empty())
					out.push_back(v);
			}
			return out;
		};

		std::filesystem::create_directories("Config");
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "FontSize" << YAML::Value << m_FontSize;
		out << YAML::Key << "FontPath" << YAML::Value << m_FontPath;
		out << YAML::Key << "ShowPhysicsColliders" << YAML::Value << m_ShowPhysicsColliders;
		out << YAML::Key << "CameraFlySpeed" << YAML::Value << m_CameraFlySpeed;
		out << YAML::Key << "ThumbnailSize" << YAML::Value << m_ThumbnailSize;
		out << YAML::Key << "ThumbnailPadding" << YAML::Value << m_ThumbnailPadding;
		out << YAML::Key << "ContentBrowserTreeWidth" << YAML::Value << m_ContentBrowserTreeWidth;
		out << YAML::Key << "ColumnWidth" << YAML::Value << m_ColumnWidth;
		out << YAML::Key << "AutoOpenLastProject" << YAML::Value << m_AutoOpenLastProject;
		out << YAML::Key << "HiddenExtensions" << YAML::Value << cleaned(m_HiddenExtensions);
		out << YAML::Key << "HiddenFolderNames" << YAML::Value << cleaned(m_HiddenFolderNames);
		out << YAML::EndMap << YAML::EndMap;
		FileSystem::Get().WriteText(GetFilePath(), out.c_str());
	}

	void EditorSettings::Load()
	{
		auto text = FileSystem::Get().ReadText(GetFilePath());
		if (!text)
			return;
		auto doc = YAML::Load(*text);
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
		if (s["CameraFlySpeed"]) m_CameraFlySpeed = s["CameraFlySpeed"].as<float>();
		if (s["ThumbnailSize"]) m_ThumbnailSize = s["ThumbnailSize"].as<float>();
		if (s["ThumbnailPadding"]) m_ThumbnailPadding = s["ThumbnailPadding"].as<float>();
		if (s["ContentBrowserTreeWidth"]) m_ContentBrowserTreeWidth = s["ContentBrowserTreeWidth"].as<float>();
		if (s["ColumnWidth"]) m_ColumnWidth = s["ColumnWidth"].as<float>();
		if (s["AutoOpenLastProject"]) m_AutoOpenLastProject = s["AutoOpenLastProject"].as<bool>();
		if (s["HiddenExtensions"]) m_HiddenExtensions = s["HiddenExtensions"].as<std::vector<std::string>>();
		if (s["HiddenFolderNames"]) m_HiddenFolderNames = s["HiddenFolderNames"].as<std::vector<std::string>>();
	}

}
