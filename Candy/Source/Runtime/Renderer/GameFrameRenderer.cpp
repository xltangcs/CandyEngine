#include "CandyPCH.h"

#include "Runtime/Renderer/GameFrameRenderer.h"
#include "Runtime/Core/Application.h"
#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/RenderCommand.h"
#include "Runtime/Renderer/Renderer2D.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Imgui/ImguiLayer.h"
#include "Runtime/UI/UISystem.h"

namespace Candy {

	void GameFrameRenderer::RenderSceneTo(Framebuffer& target, Scene& scene, EditorCamera* editorCamera)
	{
		target.Bind();
		// Note: caller is responsible for SetClearColor/Clear (and ClearAttachment for entity-pick FBO)
		// so the order between Clear and ClearAttachment(id, -1) is controlled by the caller.

		if (editorCamera)
		{
			scene.RenderScene(*editorCamera);
		}
		else
		{
			scene.RenderRuntimeScene();
		}
		// Note: caller is responsible for unbinding (to allow interleaving ReadPixel/OnOverlayRender)
	}

	void GameFrameRenderer::RenderUITo(Framebuffer& target, Scene& scene, float mouseX, float mouseY, bool mouseDown, float deltaTime)
	{
		ImGuiLayer* imgui = Application::Get().GetImGuiLayer();
		auto& spec = target.GetSpecification();

		// Target is already bound by caller
		imgui->BeginGameUI((float)spec.Width, (float)spec.Height, mouseX, mouseY, mouseDown, deltaTime);
		UISystem::RenderUI(scene);
		imgui->EndGameUI(); // Render + RenderDrawData + switch back to editor context
		// Note: caller handles unbinding
	}

}
