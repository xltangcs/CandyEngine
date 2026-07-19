#include "EditorSettingsPanel.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "Setting/EditorSettings.h"
#include "Setting/EditorState.h"
#include "Runtime/Utils/PlatformUtils.h"
#include "Runtime/Imgui/ImguiLayer.h"
#include "Runtime/Core/Application.h"

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
			float fontSize = Application::Get().GetFontSize();
			if (ImGui::SliderFloat("##FontSize", &fontSize, 12.0f, 48.0f, "%.0f px"))
			{
				Application::Get().SetFontSize(fontSize);
				ImGuiLayer::RebuildFont(Application::Get().GetFontPath());
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Font File");
			ImGui::TableSetColumnIndex(1);
			static std::string fontPath;
			static bool fontPathInit = false;
			if (!fontPathInit)
			{
				fontPath = Application::Get().GetFontPath();
				fontPathInit = true;
			}
			ImGui::InputText("##fontPath", &fontPath);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (!fontPath.empty())
				{
					Application::Get().SetFontPath(fontPath);
					ImGuiLayer::RebuildFont(fontPath);
					editorSetting.Save();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("..."))
			{
				std::string filepath = FileDialogs::OpenFile("TrueType Font (*.ttf)\0*.ttf\0");
				if (!filepath.empty())
				{
					fontPath = filepath;
					ImGuiLayer::RebuildFont(fontPath);
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
