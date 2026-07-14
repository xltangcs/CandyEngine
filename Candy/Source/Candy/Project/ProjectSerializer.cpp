#include "CandyPCH.h"
#include "Candy/Project/ProjectSerializer.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Candy {

	ProjectSerializer::ProjectSerializer(const Ref<Project>& project)
		: m_Project(project)
	{
	}

	void ProjectSerializer::Serialize(const std::filesystem::path& filepath)
	{
		const auto& settings = m_Project->GetSettings();

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << m_Project->GetName();
		out << YAML::Key << "ContentDirectory" << YAML::Value << settings.ContentDirectory;
		out << YAML::Key << "DefaultScene" << YAML::Value << settings.DefaultScene;
		out << YAML::Key << "Settings" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "DefaultWidth" << YAML::Value << settings.DefaultWidth;
		out << YAML::Key << "DefaultHeight" << YAML::Value << settings.DefaultHeight;
		out << YAML::Key << "WindowTitle" << YAML::Value << settings.WindowTitle;
		out << YAML::EndMap;
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const std::exception& e)
		{
			CANDY_CORE_ERROR("Failed to load project file '{0}': {1}", filepath.string(), e.what());
			return false;
		}

		if (!data["Project"])
		{
			CANDY_CORE_ERROR("Project file '{0}' is missing root 'Project' key", filepath.string());
			return false;
		}

		auto projectNode = data["Project"];
		m_Project->m_Name = projectNode["Name"].as<std::string>();
		m_Project->m_ProjectFileName = filepath;

		auto& settings = m_Project->m_Settings;
		if (projectNode["ContentDirectory"])
			settings.ContentDirectory = projectNode["ContentDirectory"].as<std::string>();
		if (projectNode["DefaultScene"])
			settings.DefaultScene = projectNode["DefaultScene"].as<std::string>();

		if (projectNode["Settings"])
		{
			auto settingsNode = projectNode["Settings"];
			if (settingsNode["DefaultWidth"])
				settings.DefaultWidth = settingsNode["DefaultWidth"].as<uint32_t>();
			if (settingsNode["DefaultHeight"])
				settings.DefaultHeight = settingsNode["DefaultHeight"].as<uint32_t>();
			if (settingsNode["WindowTitle"])
				settings.WindowTitle = settingsNode["WindowTitle"].as<std::string>();
		}

		return true;
	}

}
