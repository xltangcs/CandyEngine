#pragma once

#include "Runtime/Core/Base.h"

namespace Candy {

	class Scene;
	class Framebuffer;
	class EditorCamera;

	class GameFrameRenderer
	{
	public:
		static void RenderSceneTo(Framebuffer& target, Scene& scene, EditorCamera* editorCamera);
		static void RenderUITo(Framebuffer& target, Scene& scene, float mouseX, float mouseY, bool mouseDown, float deltaTime);
	};

}
