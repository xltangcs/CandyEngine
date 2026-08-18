#pragma once

#include <set>

#include "entt.hpp"

#include "Runtime/Core/Timestep.h"
#include "Runtime/Core/UUID.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Scene/SceneCamera.h"
#include "Runtime/Audio/AudioSystem.h"
#include "Runtime/Asset/StaticMeshResource.h"
#include "Runtime/Asset/Material.h"


class b2World;

namespace Candy {

	class Entity;
	class PhysicsContactListener;

	class Scene
	{
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(Ref<Scene> other);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		void QueueFree(Entity entity);
		bool IsQueuedForDeletion(Entity entity) const;
		void ProcessDeletions();

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateRuntimeLogic(Timestep ts);
		/// Begins a SceneRenderer frame and submits all draw commands (meshes,
		/// sprites, circles). The caller owns SceneRenderer::EndFrame() — the
		/// frame orchestrator (GameFrameRenderer) ends the frame once, after
		/// the debug-line overlay has also submitted.
		void RenderRuntimeScene();
		void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
		void OnUpdateSimulationLogic(Timestep ts);
		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		/// Begins a SceneRenderer frame and submits all draw commands (see
		/// RenderRuntimeScene — caller owns EndFrame()).
		void RenderScene(EditorCamera& camera);
		/// Begins a SceneRenderer frame and submits all draw commands (see
		/// RenderRuntimeScene — caller owns EndFrame()).
		void RenderSceneFromCamera(const struct CameraComponent& cameraComp, const glm::mat4& cameraTransform);

		void OnViewportResize(uint32_t width, uint32_t height);
		void DuplicateEntity(Entity entity);
		Entity GetPrimaryCameraEntity();
		void CreatePhysicsBody(Entity entity);
		/// <summary>
		/// Destroys the entity's runtime physics body (if any) and recreates it using the
		/// entity's current Transform/Collider components. Useful for resizing a collider
		/// (e.g. a crouch/shrink) at runtime. Repositions the body at the current transform.
		/// </summary>
		void RecreatePhysicsBody(Entity entity);

		const glm::vec3& GetAmbientLight() const { return m_AmbientColor; }
		void SetAmbientLight(const glm::vec3& color) { m_AmbientColor = color; }
		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		entt::registry& GetRegistry() { return m_Registry; }
		const entt::registry& GetRegistry() const { return m_Registry; }
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);

		void OnPhysics2DStart();
		void OnPhysics2DStop();

		// ---- Sprite/circle rendering (shared quad mesh + built-in materials) --
		const Ref<StaticMeshResource>& GetSpriteQuad();
		const Ref<Material>& GetSpriteMaterial();
		const Ref<Material>& GetCircleMaterial();
		/// Submits all sprite/circle entities as SceneRenderer mesh draws
		/// (transparent pass, z-ordered by SortKey = world z).
		void SubmitSpriteAndCircleDraws();
		/// Submits all StaticMeshComponent entities as SceneRenderer mesh draws
		/// (one draw per submesh, opaque/masked/transparent by material).
		void SubmitStaticMeshDraws();
		/// Sets the scene's ambient light on the renderer and submits every
		/// LightComponent entity as a SceneRenderer light.
		void SubmitSceneLights();
		/// Submits the first SkyboxComponent entity (if any) as the frame's
		/// skybox + IBL environment.
		void SubmitSkybox();
	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		b2World* m_PhysicsWorld = nullptr;
		Scope<PhysicsContactListener> m_ContactListener;
		SceneCamera m_FallbackCamera;

		std::set<entt::entity> m_PendingDeletions;

		glm::vec3 m_AmbientColor = glm::vec3(0.03f);

		// Lazily-created shared rendering resources (sprite/circle migration)
		Ref<StaticMeshResource> m_QuadMesh;
		Ref<Material> m_SpriteMaterial;
		Ref<Material> m_CircleMaterial;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}