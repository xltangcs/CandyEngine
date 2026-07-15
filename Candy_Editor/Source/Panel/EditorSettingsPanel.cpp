#include "EditorSettingsPanel.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "Candy/Project/EditorSettings.h"
#include "Candy/Project/EditorState.h"
#include "Candy/Utils/PlatformUtils.h"
#include "Candy/Imgui/ImguiLayer.h"

namespace Candy {

	void EditorSettingsPanel::OnImGuiRender()
	{
		auto& editorSetting = EditorSettings::Get();
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

}
