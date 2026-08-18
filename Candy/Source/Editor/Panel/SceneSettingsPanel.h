#pragma once

#include "Runtime/Scene/Scene.h"

namespace Candy {

	// Scene-wide settings panel (ambient light etc.), persisted into the
	// scene file (.candy). Opened from Setting > Scene Settings.
	class SceneSettingsPanel
	{
	public:
		static void OnImGuiRender(const Ref<Scene>& scene);
	};

}
