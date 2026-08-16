#include "CandyPCH.h"
#include "Runtime/Project/RecentProjects.h"

#include "Editor/Setting/EditorState.h"

#include <ctime>
#include <algorithm>

namespace Candy {

	std::vector<RecentProjectEntry> RecentProjects::Load()
	{
		EditorState::Get().Load();
		return EditorState::Get().RecentProjects;
	}

	void RecentProjects::Add(const std::string& name, const std::string& path)
	{
		auto& state = EditorState::Get();
		state.Load();

		auto& entries = state.RecentProjects;
		entries.erase(
			std::remove_if(entries.begin(), entries.end(),
				[&](const RecentProjectEntry& e) { return e.Path == path; }),
			entries.end());

		RecentProjectEntry entry;
		entry.Name = name;
		entry.Path = path;
		entry.LastOpened = std::to_string(std::time(nullptr));
		entries.insert(entries.begin(), entry);

		if (entries.size() > MaxEntries)
			entries.resize(MaxEntries);

		// Entering a project makes it the last opened one.
		state.LastOpenProject = path;

		// Persist without re-querying window geometry — this can run while the
		// compact Project Manager window is up, whose size must not overwrite
		// the editor geometry.
		state.WriteFile();
	}

}
