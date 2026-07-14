#include "CandyPCH.h"
#include "EditorSettings.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include "Candy/Utils/PlatformUtils.h"
#include "Candy/Imgui/ImguiLayer.h"
#include "EditorState.h"

namespace Candy {

	EditorSettings& EditorSettings::Get()
	{
		static EditorSettings instance;
		return instance;
	}

	void EditorSettings::Save()
	{
		std::filesystem::create_directories("Config");
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "FontSize" << YAML::Value << m_FontSize;
		out << YAML::Key << "FontPath" << YAML::Value << m_FontPath;
		out << YAML::Key << "ThumbnailSize" << YAML::Value << m_ThumbnailSize;
		out << YAML::Key << "ThumbnailPadding" << YAML::Value << m_ThumbnailPadding;
		out << YAML::Key << "AutoOpenLastProject" << YAML::Value << m_AutoOpenLastProject;
		out << YAML::EndMap << YAML::EndMap;
		std::ofstream("Config/EditorSettings.candy") << out.c_str();
	}

	void EditorSettings::OnImGuiRender()
	{
		auto& editorSetting = Get();
		auto& editorState = EditorState::Get();

		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Editor Settings", &editorState.ShowEditorSettings);

		if (ImGui::BeginTable("settings", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Font Size");
			ImGui::TableSetColumnIndex(1);
			ImGui::SliderFloat("##FontSize", &editorSetting.m_FontSize, 12.0f, 48.0f, "%.0f px");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Font File");
			ImGui::TableSetColumnIndex(1);
			ImGui::InputText("##fontPath", &editorSetting.m_FontPath);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				ImGuiLayer::RebuildFont(editorSetting.m_FontPath);
				editorSetting.Save();
			}
			ImGui::SameLine();
			if (ImGui::Button("..."))
			{
				std::string filepath = FileDialogs::OpenFile("TrueType Font (*.ttf)\0*.ttf\0");
				if (!filepath.empty())
				{
					editorSetting.m_FontPath = filepath;
					ImGuiLayer::RebuildFont(editorSetting.m_FontPath);
					editorSetting.Save();
				}
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Thumbnail Size");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::SliderFloat("##ThumbSize", &editorSetting.m_ThumbnailSize, 16, 512))
				editorSetting.Save();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Padding");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::SliderFloat("##Padding", &editorSetting.m_ThumbnailPadding, 0, 32))
				editorSetting.Save();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Auto Open Last Project");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Checkbox("##AutoOpen", &editorSetting.m_AutoOpenLastProject))
				editorSetting.Save();

			ImGui::EndTable();
		}

		ImGui::End();
	}

	void EditorSettings::Load()
	{
		auto path = std::filesystem::path("Config/EditorSettings.candy");
		if (!std::filesystem::exists(path)) return;
		auto doc = YAML::LoadFile(path.string());
		auto s = doc["EditorSettings"];
		if (!s) return;
		if (s["FontSize"]) m_FontSize = s["FontSize"].as<float>();
		if (s["FontPath"]) m_FontPath = s["FontPath"].as<std::string>();
		if (s["ThumbnailSize"]) m_ThumbnailSize = s["ThumbnailSize"].as<float>();
		if (s["ThumbnailPadding"]) m_ThumbnailPadding = s["ThumbnailPadding"].as<float>();
		if (s["AutoOpenLastProject"]) m_AutoOpenLastProject = s["AutoOpenLastProject"].as<bool>();
	}

}
