#pragma once

#include "Candy/Core/Base.h"
#include "Candy/Core/Keycodes.h"
#include "Candy/Core/MouseCodes.h"

namespace Candy {

	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};

}