#pragma once

#include <filesystem>

namespace Candy::ProjectUtils {

	std::filesystem::path GetEnginePath();
	std::filesystem::path GetEngineContentPath();
	std::filesystem::path GetEngineShadersPath();
	
	std::filesystem::path GetProjectPath();
	std::filesystem::path GetProjectContentPath();
	std::filesystem::path GetProjectConfigPath();

}
