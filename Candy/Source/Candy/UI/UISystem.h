#pragma once

#include "Candy/Core/Base.h"

struct ImVec2;

namespace Candy {

	class Scene;

	class UISystem
	{
	public:
		static void Render(Scene& scene, const ImVec2& viewportOrigin);
	};

}
