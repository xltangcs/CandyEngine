#pragma once

#include <filesystem>

namespace Candy::ProjectUtils {

	std::filesystem::path GetProjectContentPath();
	std::filesystem::path GetEngineAssetsPath();

}
