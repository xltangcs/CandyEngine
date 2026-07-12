#include "CandyPCH.h"
#include "CandyEditorSettings.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include "Candy/Utils/PlatformUtils.h"
#include "Candy/Imgui/ImguiLayer.h"
#include "CandyEditorState.h"

namespace Candy {

	CandyEditorSettings& CandyEditorSettings::Get()
	{
		static CandyEditorSettings instance;
		return instance;
	}

	void CandyEditorSettings::Save()
	{
		std::filesystem::create_directories("Config");
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "FontSize" << YAML::Value << FontSize;
		out << YAML::Key << "FontPath" << YAML::Value << FontPath;
		out << YAML::Key << "ThumbnailSize" << YAML::Value << ThumbnailSize;
		out << YAML::Key << "ThumbnailPadding" << YAML::Value << ThumbnailPadding;
		out << YAML::EndMap << YAML::EndMap;
		std::ofstream("Config/EditorSettings.yaml") << out.c_str();
	}

	void CandyEditorSettings::OnImGuiRender()
	{
		auto& editorSetting = Get();
		auto& editorState = CandyEditorState::Get();

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
			ImGui::SliderFloat("##FontSize", &editorSetting.FontSize, 12.0f, 48.0f, "%.0f px");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Font File");
			ImGui::TableSetColumnIndex(1);
			ImGui::InputText("##fontPath", &editorSetting.FontPath);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				ImGuiLayer::RebuildFont(editorSetting.FontPath);
				editorSetting.Save();
			}
			ImGui::SameLine();
			if (ImGui::Button("..."))
			{
				std::string filepath = FileDialogs::OpenFile("TrueType Font (*.ttf)\0*.ttf\0");
				if (!filepath.empty())
				{
					editorSetting.FontPath = filepath;
					ImGuiLayer::RebuildFont(editorSetting.FontPath);
					editorSetting.Save();
				}
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Thumbnail Size");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::SliderFloat("##ThumbSize", &editorSetting.ThumbnailSize, 16, 512))
				editorSetting.Save();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Padding");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::SliderFloat("##Padding", &editorSetting.ThumbnailPadding, 0, 32))
				editorSetting.Save();

			ImGui::EndTable();
		}

		ImGui::End();
	}

	void CandyEditorSettings::Load()
	{
		auto path = std::filesystem::path("Config/EditorSettings.yaml");
		if (!std::filesystem::exists(path)) return;
		auto doc = YAML::LoadFile(path.string());
		auto s = doc["EditorSettings"];
		if (!s) return;
		if (s["FontSize"]) FontSize = s["FontSize"].as<float>();
		if (s["FontPath"]) FontPath = s["FontPath"].as<std::string>();
		if (s["ThumbnailSize"]) ThumbnailSize = s["ThumbnailSize"].as<float>();
		if (s["ThumbnailPadding"]) ThumbnailPadding = s["ThumbnailPadding"].as<float>();
	}

}
