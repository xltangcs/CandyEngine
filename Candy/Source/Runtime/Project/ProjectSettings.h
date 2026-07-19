#pragma once

#include <string>

namespace Candy {

	class ProjectSettings
	{
	public:
		static ProjectSettings& Get();

		void Save();
		void Load();
		void LoadFromString(const std::string& yamlContent);

		uint32_t DefaultWidth = 1280;
		uint32_t DefaultHeight = 720;
		std::string DefaultScene = "";
		std::string GameProjectName = "CandyGame";   // premake project name to build for standalone game distribution

	private:
		ProjectSettings() = default;
	};

}
