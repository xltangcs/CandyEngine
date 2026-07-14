#include "CandyPCH.h"
#include "Candy/Project/RecentProjects.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>

namespace Candy {

	static std::filesystem::path GetFilePath()
	{
		return std::filesystem::path("Saved") / "RecentProjects.candy";
	}

	std::vector<RecentProjectEntry> RecentProjects::Load()
	{
		std::vector<RecentProjectEntry> entries;
		auto path = GetFilePath();
		if (!std::filesystem::exists(path))
			return entries;

		try
		{
			YAML::Node data = YAML::LoadFile(path.string());
			for (const auto& node : data)
			{
				RecentProjectEntry entry;
				entry.Name = node["name"].as<std::string>();
				entry.Path = node["path"].as<std::string>();
				entry.LastOpened = node["lastOpened"].as<std::string>();
				if (std::filesystem::exists(entry.Path))
					entries.push_back(entry);
			}
		}
		catch (...)
		{
			CANDY_CORE_WARN("Failed to load recent projects");
			return entries;
		}

		// Rewrite to prune invalid entries
		YAML::Emitter out;
		out << YAML::BeginSeq;
		for (const auto& e : entries)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << e.Name;
			out << YAML::Key << "path" << YAML::Value << e.Path;
			out << YAML::Key << "lastOpened" << YAML::Value << e.LastOpened;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		std::ofstream fout(path);
		fout << out.c_str();

		return entries;
	}

	void RecentProjects::Add(const std::string& name, const std::string& path)
	{
		auto filePath = GetFilePath();
		std::filesystem::create_directories(filePath.parent_path());

		auto entries = Load();

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

		YAML::Emitter out;
		out << YAML::BeginSeq;
		for (const auto& e : entries)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << e.Name;
			out << YAML::Key << "path" << YAML::Value << e.Path;
			out << YAML::Key << "lastOpened" << YAML::Value << e.LastOpened;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		std::ofstream fout(filePath);
		fout << out.c_str();
	}

}
