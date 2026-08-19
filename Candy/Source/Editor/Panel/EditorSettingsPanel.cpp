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

		ImGui::SetNextWindowSizeConstraints(ImVec2(960, 540), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Editor Settings", &editorState.ShowEditorSettings);

		if (ImGuiUtils::DrawSliderFloat("Font Size", editorSetting.m_FontSize, 12.0f, 48.0f, "%.0f px"))
		{
			ImGuiLayer::RebuildFont(editorSetting.m_FontPath);
			editorSetting.Save();
		}

		if (ImGuiUtils::DrawPathInput("Font File", editorSetting.m_FontPath))
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

		if (ImGuiUtils::DrawSliderFloat("Camera Fly Speed", editorSetting.m_CameraFlySpeed, 0.1f, 10.0f))
			editorSetting.Save();

		ImGui::Separator();
		ImGui::TextUnformatted("Content Browser Hidden Rules");
		ImGui::TextDisabled("One item per line. Folder hides by full name; extension hides files whose name ends with it (case-insensitive, no auto-dot).");

		if (ImGuiUtils::DrawMultilineStringList("Hidden Extensions", editorSetting.m_HiddenExtensions))
			editorSetting.Save();

		if (ImGuiUtils::DrawMultilineStringList("Hidden folder name", editorSetting.m_HiddenFolderNames))
			editorSetting.Save();

		ImGui::End();
	}

}
