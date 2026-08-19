#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Scene/Entity.h"

#include <glm/glm.hpp>

namespace Candy {

	class Scene;
	class Framebuffer;
	class EditorCamera;
	class RHIGraphicsPipeline;
	class RHIBuffer;

	// =========================================================================
	// EditorRenderContext — everything the editor frame render needs, filled
	// by EditorLayer (UI layer) and consumed by GameFrameRenderer (render
	// layer). The render layer never reaches back into editor UI state.
	// =========================================================================
	struct EditorRenderContext
	{
		Scene*           ActiveScene   = nullptr;
		EditorCamera*    EditorCamera  = nullptr;  ///< nullptr → render runtime (primary) camera path

		Ref<Framebuffer> ViewportTarget;           ///< main viewport framebuffer
		Ref<Framebuffer> PreviewTarget;            ///< camera-preview PIP framebuffer (used when PreviewEntity is valid)
		Entity           PreviewEntity;            ///< camera entity to preview; invalid → skip PIP pass

		bool             ShowPhysicsColliders = false;
		bool             RenderGameUI = false;     ///< Play/Simulate only

		float            UIMouseX = 0.0f, UIMouseY = 0.0f;
		bool             UIMouseDown = false;
		float            DeltaTime = 0.0f;
	};

	class GameFrameRenderer
	{
	public:
		/// Render one full editor frame: scene pass (HDR) → tonemap → camera
		/// preview PIP → game UI composite. All clear/viewport semantics are
		/// owned by SceneRenderer's render passes (LoadOp).
		static void RenderEditorFrame(const EditorRenderContext& ctx);

		static void RenderSceneTo(Framebuffer& target, Scene& scene, EditorCamera* editorCamera, float deltaTime = 1.0f / 60.0f);
		static void RenderUITo(Framebuffer& target, Scene& scene, float mouseX, float mouseY, bool mouseDown, float deltaTime);

		/// Returns the internal HDR scene target for the current frame.
		/// Entity ids live in its color attachment 1 — the editor's picking
		/// (ReadPixel) reads from here, not from the LDR display target.
		static Ref<Framebuffer> GetSceneColorTarget();

	private:
		static void RenderOverlay(const EditorRenderContext& ctx);
		static void RenderCameraPreview(const EditorRenderContext& ctx);
	};

}
