#pragma once

#include "Candy/Core/Base.h"
#include <string>
#include <vector>

namespace Candy {

	struct RecentProjectEntry
	{
		std::string Name;
		std::string Path;
		std::string LastOpened;
	};

	class RecentProjects
	{
	public:
		static std::vector<RecentProjectEntry> Load();
		static void Add(const std::string& name, const std::string& path);
		static constexpr int MaxEntries = 10;
	};

}
