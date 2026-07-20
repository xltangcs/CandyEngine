#include "ImGuiUtils.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui_internal.h>

#include "Runtime/Core/Base.h"

namespace Candy {

	float ImGuiUtils::s_ColumnWidth = 150.0f;

	bool ImGuiUtils::DrawContentPathControl(const std::string& label, std::string& path)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		ImGui::InputText("##value", &path);
		ImGui::PopItemWidth();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				path = (const char*)payload->Data;
				modified = true;
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool ImGuiUtils::DrawDragFloat(const std::string& label, float& value, float speed, float min, float max, const char* format)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		modified |= ImGui::DragFloat("##value", &value, speed, min, max, format);
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool ImGuiUtils::DrawDragFloat2(const std::string& label, glm::vec2& values, float speed, float min, float max, const char* format)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		modified |= ImGui::DragFloat2("##value", glm::value_ptr(values), speed, min, max, format);
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool ImGuiUtils::DrawColorEdit4(const std::string& label, glm::vec4& color)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		modified |= ImGui::ColorEdit4("##value", glm::value_ptr(color));
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	void ImGuiUtils::DrawLabelText(const std::string& label, const std::string& value)
	{
		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		ImGui::Text("%s", value.c_str());
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();
	}

	bool ImGuiUtils::DrawCheckbox(const std::string& label, bool& value)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		modified |= ImGui::Checkbox("##value", &value);
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool ImGuiUtils::DrawCombo(const std::string& label, const char** items, int itemCount, int& currentIndex)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		if (ImGui::BeginCombo("##value", items[currentIndex]))
		{
			for (int i = 0; i < itemCount; i++)
			{
				bool isSelected = currentIndex == i;
				if (ImGui::Selectable(items[i], isSelected))
				{
					currentIndex = i;
					modified = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool ImGuiUtils::DrawInputText(const std::string& label, std::string& value)
	{
		bool modified = false;

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1.0f);
		modified |= ImGui::InputText("##value", &value);
		ImGui::PopItemWidth();

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	void ImGuiUtils::DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, s_ColumnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFrameHeight();
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

}
