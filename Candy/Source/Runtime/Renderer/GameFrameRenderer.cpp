#include "CandyPCH.h"

#include "Runtime/Renderer/GameFrameRenderer.h"
#include "Runtime/Core/Application.h"
#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/Renderer2D.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Imgui/ImguiLayer.h"
#include "Runtime/UI/UISystem.h"

namespace Candy {

	void GameFrameRenderer::RenderSceneTo(Framebuffer& target, Scene& scene, EditorCamera* editorCamera)
	{
		target.Bind();
		// Clear/viewport semantics live in Renderer2D::Flush (LoadOp::Clear on the
		// first flush after SetActiveRenderTarget; viewport follows target size).

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

		// Target is already bound by caller
		imgui->BeginGameUI((float)target.GetWidth(), (float)target.GetHeight(), mouseX, mouseY, mouseDown, deltaTime);
		UISystem::RenderUI(scene);
		// Composite the game UI into the target framebuffer (not the swap chain).
		imgui->EndGameUI(&target); // Render + RenderDrawData + switch back to editor context
		// Note: caller handles unbinding
	}

}
