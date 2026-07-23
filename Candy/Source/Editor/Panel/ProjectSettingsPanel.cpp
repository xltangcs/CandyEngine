#include "ProjectSettingsPanel.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <filesystem>

#include "ImGuiUtils.h"
#include "Setting/EditorState.h"
#include "Runtime/Core/Application.h"
#include "Runtime/Core/VfsPath.h"
#include "Runtime/Project/Project.h"

namespace Candy {

	void ProjectSettingsPanel::OnImGuiRender()
	{
		auto project = Application::Get().GetProject();
		auto& editorState = EditorState::Get();

		ImGui::Begin("Project Settings", &editorState.ShowProjectSettings);

		if (!project)
		{
			ImGui::TextUnformatted("No project open.");
			ImGui::End();
			return;
		}

		if (ImGui::BeginTable("projectSettings", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

			// Default Scene
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Default Scene");
			ImGui::TableSetColumnIndex(1);
			std::string defaultScene = project->GetDefaultScene();
			ImGui::InputText("##DefaultScene", &defaultScene);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				project->SetDefaultScene(defaultScene);
				project->Save();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const char* path = (const char*)payload->Data;
					VfsPath vp = VfsPath::Parse(path);
					if (vp.IsValid())
					{
						std::filesystem::path relPath(vp.relativePath);
						if (relPath.extension() == ".candy")
						{
							project->SetDefaultScene(vp.ToString());
							project->Save();
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			// Default Width
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Default Width");
			ImGui::TableSetColumnIndex(1);
			int defaultWidth = (int)project->GetDefaultWidth();
			if (ImGui::InputInt("##DefaultWidth", &defaultWidth))
			{
				project->SetDefaultWidth((uint32_t)defaultWidth);
				project->Save();
			}

			// Default Height
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Default Height");
			ImGui::TableSetColumnIndex(1);
			int defaultHeight = (int)project->GetDefaultHeight();
			if (ImGui::InputInt("##DefaultHeight", &defaultHeight))
			{
				project->SetDefaultHeight((uint32_t)defaultHeight);
				project->Save();
			}

			// Game Project
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Game Project");
			ImGui::TableSetColumnIndex(1);
			std::string gameProjectName = project->GetGameProjectName();
			ImGui::InputText("##GameProjectName", &gameProjectName);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				project->SetGameProjectName(gameProjectName);
				project->Save();
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}

}
