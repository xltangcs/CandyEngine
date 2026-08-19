#pragma once

#include <set>

#include "entt.hpp"

#include "Runtime/Core/Base.h"
#include "Runtime/Core/Timestep.h"
#include "Runtime/Core/UUID.h"
#include "Runtime/Scene/SceneCamera.h"
#include "Runtime/Audio/AudioSystem.h"


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

		void OnUpdateRuntimeLogic(Timestep ts);
		void OnUpdateSimulationLogic(Timestep ts);
		/// Editor-mode logic tick (skeletal animation preview; no physics).
		void OnUpdateEditorLogic(Timestep ts);

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

		SceneCamera& GetFallbackCamera() { return m_FallbackCamera; }
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

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

	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		b2World* m_PhysicsWorld = nullptr;
		Scope<PhysicsContactListener> m_ContactListener;
		SceneCamera m_FallbackCamera;

		std::set<entt::entity> m_PendingDeletions;

		glm::vec3 m_AmbientColor = glm::vec3(0.03f);

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
