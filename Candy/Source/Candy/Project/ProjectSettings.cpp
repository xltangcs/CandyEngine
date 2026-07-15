#include "CandyPCH.h"
#include "Candy/Project/ProjectSettings.h"
#include "Candy/Project/ProjectUtils.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>

namespace Candy {

	ProjectSettings& ProjectSettings::Get()
	{
		static ProjectSettings instance;
		return instance;
	}

	void ProjectSettings::Save()
	{
		auto path = ProjectUtils::GetProjectConfigPath() / "ProjectSetting.candy";
		std::filesystem::create_directories(path.parent_path());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "ProjectSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "DefaultScene" << YAML::Value << DefaultScene;
		out << YAML::Key << "DefaultWidth" << YAML::Value << DefaultWidth;
		out << YAML::Key << "DefaultHeight" << YAML::Value << DefaultHeight;
		out << YAML::EndMap << YAML::EndMap;

		std::ofstream fout(path);
		fout << out.c_str();
	}

	void ProjectSettings::Load()
	{
		auto path = ProjectUtils::GetProjectConfigPath() / "ProjectSetting.candy";
		if (!std::filesystem::exists(path)) return;

		auto doc = YAML::LoadFile(path.string());
		auto s = doc["ProjectSettings"];
		if (!s) return;

		if (s["DefaultScene"]) DefaultScene = s["DefaultScene"].as<std::string>();
		if (s["DefaultWidth"]) DefaultWidth = s["DefaultWidth"].as<uint32_t>();
		if (s["DefaultHeight"]) DefaultHeight = s["DefaultHeight"].as<uint32_t>();
	}

}
