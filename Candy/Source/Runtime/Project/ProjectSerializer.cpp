#include "CandyPCH.h"
#include "Runtime/Project/ProjectSerializer.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Candy {

	ProjectSerializer::ProjectSerializer(const Ref<Project>& project)
		: m_Project(project)
	{
	}

	void ProjectSerializer::Serialize(const std::filesystem::path& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << m_Project->GetName();
		out << YAML::Key << "DefaultScene" << YAML::Value << m_Project->GetDefaultScene();
		out << YAML::Key << "DefaultWidth" << YAML::Value << m_Project->GetDefaultWidth();
		out << YAML::Key << "DefaultHeight" << YAML::Value << m_Project->GetDefaultHeight();
		out << YAML::Key << "GameProjectName" << YAML::Value << m_Project->GetGameProjectName();
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			CANDY_CORE_ERROR("Project file does not exist: {0}", filepath.string());
			return false;
		}

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
		if (projectNode["DefaultScene"]) m_Project->m_DefaultScene = projectNode["DefaultScene"].as<std::string>();
		if (projectNode["DefaultWidth"]) m_Project->m_DefaultWidth = projectNode["DefaultWidth"].as<uint32_t>();
		if (projectNode["DefaultHeight"]) m_Project->m_DefaultHeight = projectNode["DefaultHeight"].as<uint32_t>();
		if (projectNode["GameProjectName"]) m_Project->m_GameProjectName = projectNode["GameProjectName"].as<std::string>();
		m_Project->m_ProjectFileName = std::filesystem::absolute(filepath);

		return true;
	}

	bool ProjectSerializer::Deserialize(const std::string& yamlContent, const std::string& vfsPath)
	{
		YAML::Node data;
		try
		{
			data = YAML::Load(yamlContent);
		}
		catch (const std::exception& e)
		{
			CANDY_CORE_ERROR("Failed to parse project YAML from VFS '{0}': {1}", vfsPath, e.what());
			return false;
		}

		if (!data["Project"])
		{
			CANDY_CORE_ERROR("Project YAML is missing root 'Project' key");
			return false;
		}

		auto projectNode = data["Project"];
		m_Project->m_Name = projectNode["Name"].as<std::string>();
		if (projectNode["DefaultScene"]) m_Project->m_DefaultScene = projectNode["DefaultScene"].as<std::string>();
		if (projectNode["DefaultWidth"]) m_Project->m_DefaultWidth = projectNode["DefaultWidth"].as<uint32_t>();
		if (projectNode["DefaultHeight"]) m_Project->m_DefaultHeight = projectNode["DefaultHeight"].as<uint32_t>();
		if (projectNode["GameProjectName"]) m_Project->m_GameProjectName = projectNode["GameProjectName"].as<std::string>();
		m_Project->m_ProjectFileName = vfsPath;

		return true;
	}

}
