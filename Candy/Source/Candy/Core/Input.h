#pragma once

#include <glm/glm.hpp>
#include "Candy/Core/Keycodes.h"
#include "Candy/Core/MouseCodes.h"

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