#include "CandyPCH.h"
#include "Candy/Project/ProjectUtils.h"
#include "Candy/Core/Application.h"

namespace Candy::ProjectUtils {

	std::filesystem::path GetProjectContentPath()
	{
		auto project = Application::Get().GetProject();
		return project ? project->GetContentDirectory() : std::filesystem::path("Content");
	}

	std::filesystem::path GetEngineAssetsPath()
	{
		return std::filesystem::path("Assets");
	}

}
