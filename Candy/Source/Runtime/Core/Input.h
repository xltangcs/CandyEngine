#pragma once

#include <glm/glm.hpp>
#include "Runtime/Core/Keycodes.h"
#include "Runtime/Core/MouseCodes.h"

namespace Candy {

	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static glm::vec2 GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};

}