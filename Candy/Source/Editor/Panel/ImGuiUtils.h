#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "Setting/EditorSettings.h"

namespace Candy {

	class ImGuiUtils
	{
	public:
		static bool DrawPathInput(const std::string& label, std::string& path);
		
		template<typename TooltipFunc>
		static bool DrawPathInput(const std::string& label, std::string& path, TooltipFunc&& tooltipFunc);

		static bool DrawDragFloat(const std::string& label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawSliderFloat(const std::string& label, float& value, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawDragFloat2(const std::string& label, glm::vec2& values, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawDragFloat4(const std::string& label, glm::vec4& values, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawInputInt(const std::string& label, int& value);

		static bool DrawColorEdit4(const std::string& label, glm::vec4& color);

		static void DrawLabelText(const std::string& label, const std::string& value);

		static bool DrawCheckbox(const std::string& label, bool& value);

		static bool DrawCombo(const std::string& label, const char** items, int itemCount, int& currentIndex);

		static bool DrawInputText(const std::string& label, std::string& value);

		static bool DrawMultilineStringList(const std::string& label, std::vector<std::string>& values);

		static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f);
	};

	template <typename TooltipFunc>
	bool ImGuiUtils::DrawPathInput(const std::string& label, std::string& path, TooltipFunc&& tooltipFunc)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, EditorSettings::Get().m_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			std::forward<TooltipFunc>(tooltipFunc)();
			ImGui::EndTooltip();
		}
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		ImGui::InputText("##value", &path);
		ImGui::PopItemWidth();
		modified |= ImGui::IsItemDeactivatedAfterEdit();

		
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				path = static_cast<const char*>(payload->Data);
				modified = true;
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}
}
