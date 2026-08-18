#include "CandyPCH.h"

#include "Runtime/Renderer/GameFrameRenderer.h"
#include "Runtime/Core/Application.h"
#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/SceneRenderer.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Imgui/ImguiLayer.h"
#include "Runtime/UI/UISystem.h"

namespace Candy {

	void GameFrameRenderer::RenderEditorFrame(const EditorRenderContext& ctx)
	{
		ctx.ViewportTarget->Bind();
		SceneRenderer::SetActiveRenderTarget(ctx.ViewportTarget);
		if (ctx.ViewportTarget->GetColorAttachmentCount() > 1)
			ctx.ViewportTarget->ClearAttachment(1, -1);

		// ---- Scene pass (collect: meshes + sprites/circles) ---------------
		if (ctx.EditorCamera)
			ctx.ActiveScene->RenderScene(*ctx.EditorCamera);
		else
			ctx.ActiveScene->RenderRuntimeScene();

		// ---- Overlay pass (physics colliders etc., debug lines) -----------
		RenderOverlay(ctx);

		// ---- Unified frame submit (scene + overlay lines, one render pass) --
		SceneRenderer::EndFrame();

		// ---- Camera preview PIP -------------------------------------------
		if (ctx.PreviewTarget && ctx.PreviewEntity)
			RenderCameraPreview(ctx);

		// ---- Game UI composite --------------------------------------------
		// Only while the game is actually running (Play/Simulate). In Edit
		// mode there is no game HUD to show, and rendering it anyway made the
		// game UI present the swap chain an extra time every frame
		// (double-present → startup flicker + swap chain corruption).
		if (ctx.RenderGameUI)
			RenderUITo(*ctx.ViewportTarget, *ctx.ActiveScene, ctx.UIMouseX, ctx.UIMouseY, ctx.UIMouseDown, ctx.DeltaTime);

		ctx.ViewportTarget->Unbind();
	}

	void GameFrameRenderer::RenderSceneTo(Framebuffer& target, Scene& scene, EditorCamera* editorCamera)
	{
		target.Bind();
		// Clear semantics live in SceneRenderer::EndFrame (LoadOp::Clear on the
		// first pass after SetActiveRenderTarget; viewport follows target size).
		if (editorCamera)
		{
			scene.RenderScene(*editorCamera);
		}
		else
		{
			scene.RenderRuntimeScene();
		}
		SceneRenderer::EndFrame();
		// Note: caller is responsible for unbinding (to allow interleaving ReadPixel/OnOverlayRender)
	}

	void GameFrameRenderer::RenderOverlay(const EditorRenderContext& ctx)
	{
		if (!ctx.ShowPhysicsColliders)
			return;

		// Debug line submissions land in the frame's render pass (SceneRenderer
		// draws them after the transparent pass; camera comes from the frame's
		// BeginFrame, so no camera setup is needed here).

		// Box Colliders
		{
			auto view = ctx.ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto entity : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

				glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::scale(glm::mat4(1.0f), scale);

				SceneRenderer::SubmitRect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders: 32-segment polyline ring (replaces the old
		// thickness-0.01 SDF ring; visually equivalent, solid color, no
		// transparency overhead).
		{
			auto view = ctx.ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
			for (auto entity : view)
			{
				auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);

				glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				const glm::vec4 color(0, 1, 0, 1);
				constexpr uint32_t segments = 32;
				constexpr float twoPi = 6.28318530718f;
				for (uint32_t i = 1; i <= segments; ++i)
				{
					const float a0 = (i - 1) * twoPi / static_cast<float>(segments);
					const float a1 = i * twoPi / static_cast<float>(segments);
					const glm::vec3 p0 = glm::vec3(transform * glm::vec4(0.5f * glm::cos(a0), 0.5f * glm::sin(a0), 0.0f, 1.0f));
					const glm::vec3 p1 = glm::vec3(transform * glm::vec4(0.5f * glm::cos(a1), 0.5f * glm::sin(a1), 0.0f, 1.0f));
					SceneRenderer::SubmitLine(p0, p1, color);
				}
			}
		}
	}

	void GameFrameRenderer::RenderCameraPreview(const EditorRenderContext& ctx)
	{
		Entity previewEntity = ctx.PreviewEntity; // mutable copy — Entity is a registry handle
		auto& cameraComp = previewEntity.GetComponent<CameraComponent>();
		auto& cameraTransform = previewEntity.GetComponent<TransformComponent>();

		// Set camera viewport to match preview framebuffer size
		auto& sceneCamera = cameraComp.Camera;
		sceneCamera.SetViewportSize(ctx.PreviewTarget->GetWidth(), ctx.PreviewTarget->GetHeight());

		ctx.PreviewTarget->Bind();
		SceneRenderer::SetActiveRenderTarget(ctx.PreviewTarget);

		ctx.ActiveScene->RenderSceneFromCamera(cameraComp, cameraTransform.GetTransform());
		SceneRenderer::EndFrame();

		ctx.PreviewTarget->Unbind();
		ctx.ViewportTarget->Bind(); // Re-bind main FBO for subsequent UI rendering

		// Restore main viewport framebuffer as the active render target.
		SceneRenderer::SetActiveRenderTarget(ctx.ViewportTarget);

		// Restore camera viewport to main viewport size
		sceneCamera.SetViewportSize(ctx.ViewportTarget->GetWidth(), ctx.ViewportTarget->GetHeight());
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
