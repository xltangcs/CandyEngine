#include "CandyPCH.h"
#include "Runtime/Project/Project.h"
#include "Runtime/Project/ProjectSerializer.h"

namespace Candy {

	Ref<Project> Project::New(const std::filesystem::path& directory, const std::string& name)
	{
		std::filesystem::create_directories(directory);
		std::filesystem::create_directories(directory / "Content");

		Ref<Project> project = Ref<Project>(new Project());
		project->m_ProjectName = name;
		project->m_ProjectFileName = directory / (name + ".candyproj");

		project->Save();
		return project;
	}

	Ref<Project> Project::Load(const std::filesystem::path& projectFile)
	{
		Ref<Project> project = Ref<Project>(new Project());
		ProjectSerializer serializer(project);
		if (!serializer.Deserialize(projectFile))
		{
			CANDY_CORE_ERROR("Failed to deserialize project file: {0}", projectFile.string());
			return nullptr;
		}
		return project;
	}

	Ref<Project> Project::LoadFromVfs(const std::string& yamlContent, const std::string& vfsProjectPath)
	{
		Ref<Project> project = Ref<Project>(new Project());
		ProjectSerializer serializer(project);
		if (!serializer.Deserialize(yamlContent, vfsProjectPath))
		{
			CANDY_CORE_ERROR("Failed to deserialize project from VFS: {0}", vfsProjectPath);
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
