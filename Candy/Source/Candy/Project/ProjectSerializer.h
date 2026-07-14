#pragma once

#include "Candy/Core/Base.h"
#include "Candy/Project/Project.h"

namespace Candy {

	class ProjectSerializer
	{
	public:
		ProjectSerializer(const Ref<Project>& project);

		void Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);
	private:
		Ref<Project> m_Project;
	};

}
