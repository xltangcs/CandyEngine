#include "CandyPCH.h"
#include "CandyEditorState.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <GLFW/glfw3.h>
#include "Candy/Core/Application.h"

namespace Candy {

	CandyEditorState& CandyEditorState::Get()
	{
		static CandyEditorState instance;
		return instance;
	}

	void CandyEditorState::Save()
	{
		std::filesystem::create_directories("Saved");
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		int w, h;
		glfwGetWindowSize(window, &w, &h);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorState" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "LastScenePath" << YAML::Value << LastScenePath;
		out << YAML::Key << "WindowWidth" << YAML::Value << w;
		out << YAML::Key << "WindowHeight" << YAML::Value << h;
		out << YAML::Key << "WindowMaximized" << YAML::Value << (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
		out << YAML::EndMap << YAML::EndMap;
		std::ofstream("Saved/EditorState.yaml") << out.c_str();
	}

	void CandyEditorState::Load()
	{
		auto path = std::filesystem::path("Saved/EditorState.yaml");
		if (!std::filesystem::exists(path)) return;
		auto doc = YAML::LoadFile(path.string());
		auto s = doc["EditorState"];
		if (!s) return;
		if (s["LastScenePath"]) LastScenePath = s["LastScenePath"].as<std::string>();
		if (s["WindowWidth"]) WindowWidth = s["WindowWidth"].as<int>();
		if (s["WindowHeight"]) WindowHeight = s["WindowHeight"].as<int>();
		if (s["WindowMaximized"]) WindowMaximized = s["WindowMaximized"].as<bool>();
	}

}
