#include "CandyPCH.h"
#include "Candy/Project/Project.h"
#include "Candy/Project/ProjectSerializer.h"

namespace Candy {

	Ref<Project> Project::New(const std::filesystem::path& directory, const std::string& name)
	{
		std::filesystem::create_directories(directory);
		std::filesystem::create_directories(directory / "Content");

		Ref<Project> project = Ref<Project>(new Project());
		project->m_Name = name;
		project->m_ProjectFileName = directory / (name + ".candyproj");

		project->Save();
		return project;
	}

	Ref<Project> Project::Load(const std::filesystem::path& projectFile)
	{
		if (!std::filesystem::exists(projectFile))
		{
			CANDY_CORE_ERROR("Project file does not exist: {0}", projectFile.string());
			return nullptr;
		}

		Ref<Project> project = Ref<Project>(new Project());
		ProjectSerializer serializer(project);
		if (!serializer.Deserialize(projectFile))
		{
			CANDY_CORE_ERROR("Failed to deserialize project file: {0}", projectFile.string());
			return nullptr;
		}
		return project;
	}

	void Project::Save()
	{
		ProjectSerializer serializer(shared_from_this());
		serializer.Serialize(m_ProjectFileName);
	}

}
