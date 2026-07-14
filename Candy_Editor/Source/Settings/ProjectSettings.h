#pragma once

#include <string>

namespace Candy {

	class ProjectSettings
	{
	public:
		static ProjectSettings& Get();

		void OnImGuiRender();
		void Save();
		void Load();

		uint32_t DefaultWidth = 1280;
		uint32_t DefaultHeight = 720;
		std::string DefaultScene = "";

	private:
		ProjectSettings() = default;
	};

}
