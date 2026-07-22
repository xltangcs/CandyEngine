#pragma once

#include "Runtime/Core/Base.h"

#include <filesystem>
#include <string>

namespace Candy {

	class Project : public std::enable_shared_from_this<Project>
	{
	public:
		static Ref<Project> New(const std::filesystem::path& directory, const std::string& name);
		static Ref<Project> Load(const std::filesystem::path& projectFile);
		static Ref<Project> LoadFromVfs(const std::string& yamlContent, const std::string& vfsProjectPath);
		void Save();

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		const std::filesystem::path& GetProjectFileName() const { return m_ProjectFileName; }
		std::filesystem::path GetProjectDirectory() const { return m_ProjectFileName.parent_path(); }
		std::filesystem::path GetFullStartScenePath() const { return GetProjectDirectory() / "Content" / m_DefaultScene; }

		const std::string& GetDefaultScene() const { return m_DefaultScene; }
		void SetDefaultScene(const std::string& scene) { m_DefaultScene = scene; }

		friend class ProjectSerializer;

	private:
		Project() = default;

		std::string m_Name;
		std::string m_DefaultScene;
		std::filesystem::path m_ProjectFileName;
	};

}
