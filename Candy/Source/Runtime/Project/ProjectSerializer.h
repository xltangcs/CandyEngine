#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Project/Project.h"

namespace Candy {

	class ProjectSerializer
	{
	public:
		ProjectSerializer(const Ref<Project>& project);

		void Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::string& yamlContent, const std::string& vfsPath);
	private:
		Ref<Project> m_Project;
	};

}
