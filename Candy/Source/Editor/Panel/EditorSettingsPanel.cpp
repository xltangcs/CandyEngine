#include "EditorSettingsPanel.h"

#include <imgui/imgui.h>

#include "ImGuiUtils.h"
#include "Setting/EditorSettings.h"
#include "Setting/EditorState.h"
#include "Runtime/Imgui/ImguiLayer.h"

namespace Candy {

	void EditorSettingsPanel::OnImGuiRender()
	{
		auto& editorSetting = EditorSettings::Get();
		auto& editorState = EditorState::Get();

		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Editor Settings", &editorState.ShowEditorSettings);

		if (ImGuiUtils::DrawSliderFloat("Font Size", editorSetting.m_FontSize, 12.0f, 48.0f, "%.0f px"))
		{
			ImGuiLayer::RebuildFont(editorSetting.m_FontPath);
			editorSetting.Save();
		}

		if (ImGuiUtils::DrawContentPathControl("Font File", editorSetting.m_FontPath))
		{
			ImGuiLayer::RebuildFont(editorSetting.m_FontPath);
			editorSetting.Save();
		}

		if (ImGuiUtils::DrawSliderFloat("Thumbnail Size", editorSetting.m_ThumbnailSize, 16.0f, 512.0f))
			editorSetting.Save();

		if (ImGuiUtils::DrawSliderFloat("Padding", editorSetting.m_ThumbnailPadding, 0.0f, 32.0f))
			editorSetting.Save();

		if (ImGuiUtils::DrawSliderFloat("Column Width", editorSetting.m_ColumnWidth, 80.0f, 400.0f, "%.0f px"))
			editorSetting.Save();

		if (ImGuiUtils::DrawCheckbox("Auto Open Last Project", editorSetting.m_AutoOpenLastProject))
			editorSetting.Save();

		if (ImGuiUtils::DrawCheckbox("Show Physics Colliders", editorSetting.m_ShowPhysicsColliders))
			editorSetting.Save();

		ImGui::End();
	}

}
