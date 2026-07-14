#include "CandyPCH.h"
#include "Candy/Project/ProjectUtils.h"
#include "Candy/Core/Application.h"

namespace Candy::ProjectUtils {
	
	std::filesystem::path GetEnginePath()
	{
		return std::filesystem::path("..") / "Candy";
	}

	std::filesystem::path GetEngineContentPath()
	{
		return GetEnginePath() / "Content";
	}

	std::filesystem::path GetEngineShadersPath()
	{
		return GetEnginePath() / "Content" / "Shaders";
	}

	std::filesystem::path GetProjectPath()
	{
		auto project = Application::Get().GetProject();
		return project ? project->GetProjectDirectory() : std::filesystem::current_path();
	}

	std::filesystem::path GetProjectContentPath()
	{
		return GetProjectPath() / "Content";
	}

	std::filesystem::path GetProjectConfigPath()
	{
		return GetProjectPath() / "Config";
	}
}
