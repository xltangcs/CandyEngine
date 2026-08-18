#include "SceneSettingsPanel.h"

#include <imgui/imgui.h>

#include "ImGuiUtils.h"
#include "Setting/EditorState.h"

namespace Candy {

	void SceneSettingsPanel::OnImGuiRender(const Ref<Scene>& scene)
	{
		auto& editorState = EditorState::Get();

		ImGui::SetNextWindowSizeConstraints(ImVec2(480, 320), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Scene Settings", &editorState.ShowSceneSettings);

		if (!scene)
		{
			ImGui::TextUnformatted("No scene open.");
			ImGui::End();
			return;
		}

		// Ambient Color: scene-wide indirect-lighting tint, serialized
		// into the .candy file by SceneSerializer.
		glm::vec3 ambient = scene->GetAmbientLight();
		if (ImGuiUtils::DrawColorEdit3("Ambient Color", ambient))
			scene->SetAmbientLight(ambient);

		ImGui::End();
	}

}
