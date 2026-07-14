#pragma once

#include "Candy/Core/Base.h"

#include <filesystem>
#include <string>

#include "../../../../Candy_Editor/Source/Settings/ProjectSettings.h"

namespace Candy
{
	class ProjectSettings;
}

namespace Candy {

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
		std::filesystem::path GetFullStartScenePath() const { return GetProjectDirectory() / "Content" / GetProjectDefaultScene(); }

		static const std::string& GetProjectDefaultScene() { return ProjectSettings::Get().DefaultScene; }

		friend class ProjectSerializer;

	private:
		Project() = default;

		std::string m_Name;
		std::filesystem::path m_ProjectFileName;
	};

}
