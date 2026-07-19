#include "ProjectSettingsPanel.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <filesystem>

#include "Runtime/Project/ProjectSettings.h"
#include "Setting/EditorState.h"

namespace Candy {

	void ProjectSettingsPanel::OnImGuiRender()
	{
		auto& settings = ProjectSettings::Get();
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
				settings.Save();
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

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Game Project");
			ImGui::TableSetColumnIndex(1);
			ImGui::InputText("##GameProjectName", &settings.GameProjectName);
			if (ImGui::IsItemDeactivatedAfterEdit())
				settings.Save();
			ImGui::EndTable();
		}

		ImGui::End();
	}

}
