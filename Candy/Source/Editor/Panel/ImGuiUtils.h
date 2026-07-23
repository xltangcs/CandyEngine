#pragma once

#include <string>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace Candy {

	class ImGuiUtils
	{
	public:
		static bool DrawContentPathControl(const std::string& label, std::string& path);

		static bool DrawDragFloat(const std::string& label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawSliderFloat(const std::string& label, float& value, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawDragFloat2(const std::string& label, glm::vec2& values, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

		static bool DrawColorEdit4(const std::string& label, glm::vec4& color);

		static void DrawLabelText(const std::string& label, const std::string& value);

		static bool DrawCheckbox(const std::string& label, bool& value);

		static bool DrawCombo(const std::string& label, const char** items, int itemCount, int& currentIndex);

		static bool DrawInputText(const std::string& label, std::string& value);

		static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f);
	};

}
