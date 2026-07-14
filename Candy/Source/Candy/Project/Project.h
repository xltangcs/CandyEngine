#pragma once

#include "Candy/Core/Base.h"

#include <filesystem>
#include <map>
#include <string>

namespace Candy {

	struct ProjectSettings
	{
		std::string ContentDirectory = "Content";
		std::string DefaultScene = "Scenes/Default.candy";
		uint32_t DefaultWidth = 1280;
		uint32_t DefaultHeight = 720;
		std::string WindowTitle = "";
		std::map<std::string, std::string> CustomSettings;
	};

	class Project : public std::enable_shared_from_this<Project>
	{
	public:
		static Ref<Project> New(const std::filesystem::path& directory, const std::string& name);
		static Ref<Project> Load(const std::filesystem::path& projectFile);
		void Save();

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		const std::filesystem::path& GetProjectFileName() const { return m_ProjectFileName; }
		std::filesystem::path GetProjectDirectory() const { return m_ProjectFileName.parent_path(); }
		std::filesystem::path GetContentDirectory() const { return GetProjectDirectory() / m_Settings.ContentDirectory; }
		std::filesystem::path GetFullStartScenePath() const { return GetContentDirectory() / m_Settings.DefaultScene; }

		ProjectSettings& GetSettings() { return m_Settings; }
		const ProjectSettings& GetSettings() const { return m_Settings; }

		friend class ProjectSerializer;

	private:
		Project() = default;

		std::string m_Name;
		std::filesystem::path m_ProjectFileName;
		ProjectSettings m_Settings;
	};

}
