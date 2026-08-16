#include "CandyPCH.h"
#include "EditorState.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <GLFW/glfw3.h>
#include "Runtime/Core/Application.h"

namespace Candy {

	static std::filesystem::path GetFilePath()
	{
		return std::filesystem::path("Saved") / "EditorState.candy";
	}

	EditorState& EditorState::Get()
	{
		static EditorState instance;
		return instance;
	}

	void EditorState::Load()
	{
		// Only load from disk once; afterwards in-memory state is authoritative
		// (so a mid-session RecentProjects::Add is not clobbered by a re-load).
		if (m_Loaded)
			return;
		m_Loaded = true;

		auto path = GetFilePath();
		if (!std::filesystem::exists(path))
			return;

		auto doc = YAML::LoadFile(path.string());
		auto s = doc["EditorState"];
		if (!s)
			return;

		if (s["LastOpenProject"]) LastOpenProject = s["LastOpenProject"].as<std::string>();
		if (s["WindowWidth"]) WindowWidth = s["WindowWidth"].as<int>();
		if (s["WindowHeight"]) WindowHeight = s["WindowHeight"].as<int>();
		if (s["WindowMaximized"]) WindowMaximized = s["WindowMaximized"].as<bool>();
		if (s["LayoutPresetApplied"]) LayoutPresetApplied = s["LayoutPresetApplied"].as<bool>();

		if (auto recents = s["RecentProjects"])
		{
			for (const auto& node : recents)
			{
				RecentProjectEntry entry;
				entry.Name = node["name"].as<std::string>();
				entry.Path = node["path"].as<std::string>();
				entry.LastOpened = node["lastOpened"].as<std::string>();
				if (std::filesystem::exists(entry.Path))
					RecentProjects.push_back(entry);
			}
		}
	}

	void EditorState::Save()
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		WindowWidth = w;
		WindowHeight = h;
		WindowMaximized = (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

		WriteFile();
	}

	void EditorState::WriteFile()
	{
		std::filesystem::create_directories(GetFilePath().parent_path());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorState" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
		for (const auto& e : RecentProjects)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << e.Name;
			out << YAML::Key << "path" << YAML::Value << e.Path;
			out << YAML::Key << "lastOpened" << YAML::Value << e.LastOpened;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::Key << "LastOpenProject" << YAML::Value << LastOpenProject;
		out << YAML::Key << "WindowWidth" << YAML::Value << WindowWidth;
		out << YAML::Key << "WindowHeight" << YAML::Value << WindowHeight;
		out << YAML::Key << "WindowMaximized" << YAML::Value << WindowMaximized;
		out << YAML::Key << "LayoutPresetApplied" << YAML::Value << LayoutPresetApplied;
		out << YAML::EndMap << YAML::EndMap;
		std::ofstream fout(GetFilePath());
		fout << out.c_str();
	}

}
