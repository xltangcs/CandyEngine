#pragma once

#include "Runtime/Core/Base.h"

#include <glm/glm.hpp>

namespace Candy {

	class Scene;
	class EditorCamera;
	struct CameraComponent;

	// =========================================================================
	// SceneDrawCollector — reads the scene's ECS registry and submits draw
	// commands to SceneRenderer. This is the renderer-side view of the scene
	// (a lightweight stand-in for UE's FScene): Scene itself knows nothing
	// about the renderer, keeping the dependency direction one-way.
	//
	// Callers own SceneRenderer::EndFrame() — this collector only begins the
	// frame and submits; the frame orchestrator (GameFrameRenderer) ends it
	// once, after the debug-line overlay has also submitted.
	// =========================================================================
	class SceneDrawCollector
	{
	public:
		static void SubmitScene(Scene& scene, EditorCamera& camera);
		static void SubmitScene(Scene& scene, const CameraComponent& cameraComp, const glm::mat4& cameraTransform);
		static void SubmitRuntimeScene(Scene& scene);

	private:
		static void SubmitSpriteAndCircleDraws(Scene& scene);
		static void SubmitStaticMeshDraws(Scene& scene);
		static void SubmitSkeletalMeshDraws(Scene& scene);
		static void SubmitSceneLights(Scene& scene);
		static void SubmitSkybox(Scene& scene);

		// Shared built-in resources (sprite/circle quad + materials), created
		// once and shared across all scenes (read-only).
		static const Ref<class StaticMeshResource>& GetSpriteQuad();
		static const Ref<class Material>& GetSpriteMaterial();
		static const Ref<class Material>& GetCircleMaterial();
	};

} // namespace Candy
