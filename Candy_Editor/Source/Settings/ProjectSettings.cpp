#include "CandyPCH.h"
#include "ProjectSettings.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include "EditorState.h"
#include "Candy/Core/Application.h"
#include "Candy/Project/Project.h"
#include "Candy/Project/ProjectUtils.h"

namespace Candy {

	ProjectSettings& ProjectSettings::Get()
	{
		static ProjectSettings instance;
		return instance;
	}

	void ProjectSettings::OnImGuiRender()
	{
		auto& settings = Get();
		auto& editorState = EditorState::Get();

		ImGui::Begin("Project Settings", &editorState.ShowProjectSettings);

		if (ImGui::BeginTable("projectSettings", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Default Scene");
			ImGui::TableSetColumnIndex(1);
			ImGui::InputText("##DefaultScene", &settings.DefaultScene);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				settings.Save();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					settings.DefaultScene = std::filesystem::path(path).string();
					settings.Save();
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Default Width");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::InputInt("##DefaultWidth", (int*)&settings.DefaultWidth))
				settings.Save();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Default Height");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::InputInt("##DefaultHeight", (int*)&settings.DefaultHeight))
				settings.Save();

			ImGui::EndTable();
		}

		ImGui::End();
	}

	void ProjectSettings::Save()
	{
		auto path = ProjectUtils::GetProjectConfigPath() / "ProjectSetting.candy";
		std::filesystem::create_directories(path.parent_path());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "ProjectSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "DefaultScene" << YAML::Value << DefaultScene;
		out << YAML::Key << "DefaultWidth" << YAML::Value << DefaultWidth;
		out << YAML::Key << "DefaultHeight" << YAML::Value << DefaultHeight;
		out << YAML::EndMap << YAML::EndMap;

		std::ofstream fout(path);
		fout << out.c_str();
	}

	void ProjectSettings::Load()
	{
		auto path = ProjectUtils::GetProjectConfigPath() / "ProjectSetting.candy";
		if (!std::filesystem::exists(path)) return;

		auto doc = YAML::LoadFile(path.string());
		auto s = doc["ProjectSettings"];
		if (!s) return;

		if (s["DefaultScene"]) DefaultScene = s["DefaultScene"].as<std::string>();
		if (s["DefaultWidth"]) DefaultWidth = s["DefaultWidth"].as<uint32_t>();
		if (s["DefaultHeight"]) DefaultHeight = s["DefaultHeight"].as<uint32_t>();
	}

}
