#include "CandyPCH.h"

#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Entity.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Scene/ScriptableEntity.h"
#include "Runtime/Scene/PhysicsContactListener.h"
#include "Runtime/Scene/SkeletalAnimationSystem.h"
#include "Runtime/Scripting/ScriptSystem.h"

#include "Runtime/Renderer/SceneRenderer.h"
#include "Runtime/Renderer/Texture.h"

#include <glm/glm.hpp>

// Box2D
#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"


namespace Candy {

	static b2BodyType Rigidbody2DTypeToBox2DBody(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
			case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
			case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
			case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
		}

		CANDY_CORE_ASSERT(false, "Unknown body type");
		return b2_staticBody;
	}



	Scene::Scene()
	{
		m_FallbackCamera = SceneCamera::CreateOrthographic(10.0f, 0.1f, 1000.0f);
	}

	Scene::~Scene()
	{
		delete m_PhysicsWorld;
	}

	template<typename Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto view = src.view<Component>();
		for (auto e : view)
		{
			UUID uuid = src.get<IDComponent>(e).ID;
			CANDY_CORE_ASSERT(enttMap.find(uuid) != enttMap.end())
			entt::entity dstEnttID = enttMap.at(uuid);

			auto& component = src.get<Component>(e);
			dst.emplace_or_replace<Component>(dstEnttID, component);
		}
	}

	template<typename Component>
	static void CopyComponentIfExists(Entity dst, Entity src)
	{
		if (src.HasComponent<Component>())
			dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other)
	{
		Ref<Scene> newScene = CreateRef<Scene>();

		newScene->m_ViewportWidth = other->m_ViewportWidth;
		newScene->m_ViewportHeight = other->m_ViewportHeight;
		newScene->m_AmbientColor = other->m_AmbientColor;

		auto& srcSceneRegistry = other->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;
		std::unordered_map<UUID, entt::entity> enttMap;

		// Create entities in new scene (same order as hierarchy panel: reverse creation order)
		for (auto [enttID] : srcSceneRegistry.storage<entt::entity>().reach())
		{
			Entity entity = { enttID, other.get() };
			Entity newEntity = newScene->CreateEntityWithUUID(entity.GetUUID(), entity.GetName());
			enttMap[entity.GetUUID()] = (entt::entity)newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<StaticMeshComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SkeletalMeshComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<LightComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SkyboxComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<AudioSourceComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<UITextBlockComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<UIButtonComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

		// Per-instance runtime resources must not be shared across scenes:
		// reset the copied bone CBs so each instance lazily re-creates its own.
		for (auto&& [enttID, smc] : dstSceneRegistry.view<SkeletalMeshComponent>().each())
		{
			smc.BoneBuffer = nullptr;
			smc.BoneBufferJoints = 0;
		}

		return newScene;
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string & name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (entity.HasComponent<ScriptComponent>())
			ScriptSystem::Get().DestroyScript(entity.GetUUID());

		if (m_PhysicsWorld && entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			if (rb2d.RuntimeBody)
			{
				m_PhysicsWorld->DestroyBody((b2Body*)rb2d.RuntimeBody);
				rb2d.RuntimeBody = nullptr;
			}
		}

		m_Registry.destroy(entity);
	}

	void Scene::QueueFree(Entity entity)
	{
		if (!entity || !m_Registry.valid(entity))
			return;
		m_PendingDeletions.insert(entity);
	}

	bool Scene::IsQueuedForDeletion(Entity entity) const
	{
		return m_PendingDeletions.count(entity) > 0;
	}

	void Scene::ProcessDeletions()
	{
		if (m_PendingDeletions.empty())
			return;

		// Iterate over the set without modifying it (only clear() at the end),
		// so traversal is never invalidated.
		for (entt::entity e : m_PendingDeletions)
		{
			Entity entity{ e, this };
			if (entity)
				DestroyEntity(entity);
		}
		m_PendingDeletions.clear();
	}

	void Scene::OnRuntimeStart()
	{
		auto& scriptSystem = ScriptSystem::Get();

		auto scriptView = m_Registry.view<ScriptComponent>();
		for (auto e : scriptView)
		{
			Entity entity{ e, this };
			scriptSystem.InstantiateScript(entity);
		}

		scriptSystem.OnRuntimeStart();
		AudioSystem::OnRuntimeStart(*this);
		OnPhysics2DStart();
	}

	void Scene::OnRuntimeStop()
	{
		OnPhysics2DStop();
		AudioSystem::OnRuntimeStop(*this);
		ScriptSystem::Get().OnRuntimeStop();
	}

	void Scene::OnSimulationStart()
	{
		OnPhysics2DStart();
	}

	void Scene::OnSimulationStop()
	{
		OnPhysics2DStop();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		OnUpdateRuntimeLogic(ts);
		RenderRuntimeScene();
		SceneRenderer::EndFrame();
	}

	void Scene::OnUpdateRuntimeLogic(Timestep ts)
	{
		ProcessDeletions();

		// Update Python scripts
		ScriptSystem::Get().OnUpdateRuntime(ts);

		// Update audio
		AudioSystem::OnUpdateRuntime(*this, ts);

		// Update native scripts
		{
			m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
				{
					if (!nsc.Instance)
					{
						nsc.Instance = nsc.InstantiateScript();
						nsc.Instance->m_Entity = Entity{ entity, this };
						nsc.Instance->OnCreate();
					}

					nsc.Instance->OnUpdate(ts);
				});
		}

		// Physics
		{
			const int32_t velocityIterations = 6;
			const int32_t positionIterations = 2;
			m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

			// Process collision events
			if (m_ContactListener)
				m_ContactListener->Flush();

			// Retrieve transform from Box2D
			auto view = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetComponent<TransformComponent>();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				const auto& position = body->GetPosition();
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				transform.Rotation.z = body->GetAngle();
			}
		}

		// Animation evaluation last: scripts/physics may have changed
		// Time/ClipName this tick; the renderer consumes the fresh pose.
		SkeletalAnimationSystem::Update(*this, ts);
	}

	// ---- Sprite/circle shared rendering resources ---------------------------

	const Ref<StaticMeshResource>& Scene::GetSpriteQuad()
	{
		if (!m_QuadMesh)
			m_QuadMesh = StaticMeshResource::CreateQuad();
		return m_QuadMesh;
	}

	const Ref<Material>& Scene::GetSpriteMaterial()
	{
		if (!m_SpriteMaterial)
		{
			m_SpriteMaterial = CreateRef<Material>();
			m_SpriteMaterial->Name = "Builtin/Sprite";
			m_SpriteMaterial->ShaderPath = "VFS://Engine/Content/Shaders/D3D12/Sprite.hlsl";
			m_SpriteMaterial->ShaderParams["u_BlendMode"] = 2.0f; // Transparent
		}
		return m_SpriteMaterial;
	}

	const Ref<Material>& Scene::GetCircleMaterial()
	{
		if (!m_CircleMaterial)
		{
			m_CircleMaterial = CreateRef<Material>();
			m_CircleMaterial->Name = "Builtin/Circle";
			m_CircleMaterial->ShaderPath = "VFS://Engine/Content/Shaders/D3D12/Sprite.hlsl";
			m_CircleMaterial->ShaderParams["u_BlendMode"]  = 2.0f; // Transparent
			m_CircleMaterial->ShaderParams["u_CircleMode"] = 1.0f; // SDF circle
		}
		return m_CircleMaterial;
	}

	void Scene::SubmitSpriteAndCircleDraws()
	{
		const auto& quad = GetSpriteQuad();

		// Sprites
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [tc, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				const glm::mat4 transform = tc.GetTransform();

				MeshDrawCommand draw;
				draw.Transform    = transform;
				draw.Mesh         = quad;
				draw.Material     = GetSpriteMaterial();
				draw.EntityID     = static_cast<int>(entity);
				draw.SortKey      = transform[3].z; // 2D z-order (smaller = further back)
				draw.HasOverrides = true;
				draw.Overrides.BaseColor = sprite.Color;
				draw.Overrides.UVTiling  = glm::vec2(sprite.TilingFactor);
				if (sprite.Texture)
					draw.Overrides.BaseColorTexture = sprite.Texture;
				else if (!sprite.TexturePath.empty())
					draw.Overrides.BaseColorMap = sprite.TexturePath;
				SceneRenderer::Submit(draw);
			}
		}

		// Circles
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				auto [tc, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
				const glm::mat4 transform = tc.GetTransform();

				MeshDrawCommand draw;
				draw.Transform    = transform;
				draw.Mesh         = quad;
				draw.Material     = GetCircleMaterial();
				draw.EntityID     = static_cast<int>(entity);
				draw.SortKey      = transform[3].z;
				draw.HasOverrides = true;
				draw.Overrides.BaseColor  = circle.Color;
				draw.Overrides.CircleMode = 1.0f;
				draw.Overrides.Thickness  = circle.Thickness;
				draw.Overrides.Fade       = circle.Fade;
				SceneRenderer::Submit(draw);
			}
		}
	}

	void Scene::RenderRuntimeScene()
	{
		// Render 2D (sprites/circles) via the unified scene renderer.
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				mainCamera = &camera.Camera;
				cameraTransform = transform.GetTransform();
				break;
			}
		}

		if (!mainCamera)
		{
			mainCamera = &m_FallbackCamera;
			cameraTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			m_FallbackCamera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}

		{
			SceneRenderer::BeginFrame(*mainCamera, cameraTransform);
			SubmitSkybox();
			SubmitSceneLights();
			SubmitStaticMeshDraws();
			SubmitSkeletalMeshDraws();
			SubmitSpriteAndCircleDraws();
		}
	}

	void Scene::OnUpdateSimulationLogic(Timestep ts)
	{
		ProcessDeletions();

		// Physics
		{
			const int32_t velocityIterations = 6;
			const int32_t positionIterations = 2;
			m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

			// Retrieve transform from Box2D
			auto view = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetComponent<TransformComponent>();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				const auto& position = body->GetPosition();
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				transform.Rotation.z = body->GetAngle();
			}
		}

		// Animation evaluation last: scripts/physics may have changed
		// Time/ClipName this tick; the renderer consumes the fresh pose.
		SkeletalAnimationSystem::Update(*this, ts);
	}

	void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& camera)
	{
		OnUpdateSimulationLogic(ts);

		// Render
		RenderScene(camera);
		SceneRenderer::EndFrame();
	}

	void Scene::OnUpdateEditorLogic(Timestep ts)
	{
		ProcessDeletions();

		// Edit mode has no physics; only presentation-adjacent runtime state
		// (skeletal animation preview) advances here.
		SkeletalAnimationSystem::Update(*this, ts);
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
	{
		// Render
		RenderScene(camera);
		SceneRenderer::EndFrame();
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize fallback camera
		m_FallbackCamera.SetViewportSize(width, height);

		// Resize our non-FixedAspectRatio cameras
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	void Scene::DuplicateEntity(Entity entity)
	{
		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);

		CopyComponentIfExists<TransformComponent>(newEntity, entity);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity, entity);
		CopyComponentIfExists<StaticMeshComponent>(newEntity, entity);
		CopyComponentIfExists<SkeletalMeshComponent>(newEntity, entity);
		CopyComponentIfExists<CircleRendererComponent>(newEntity, entity);
		CopyComponentIfExists<CameraComponent>(newEntity, entity);
		CopyComponentIfExists<LightComponent>(newEntity, entity);
		CopyComponentIfExists<SkyboxComponent>(newEntity, entity);
		CopyComponentIfExists<NativeScriptComponent>(newEntity, entity);
		CopyComponentIfExists<Rigidbody2DComponent>(newEntity, entity);
		CopyComponentIfExists<BoxCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<CircleCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<ScriptComponent>(newEntity, entity);
		CopyComponentIfExists<AudioSourceComponent>(newEntity, entity);
		CopyComponentIfExists<UITextBlockComponent>(newEntity, entity);
		CopyComponentIfExists<UIButtonComponent>(newEntity, entity);

		// Per-instance runtime resources must not be shared: the duplicate
		// lazily re-creates its own bone CB on the next animation tick.
		if (newEntity.HasComponent<SkeletalMeshComponent>())
		{
			auto& smc = newEntity.GetComponent<SkeletalMeshComponent>();
			smc.BoneBuffer = nullptr;
			smc.BoneBufferJoints = 0;
		}
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			return Entity{ entity, this };
		}
		return {};
	}

	void Scene::OnPhysics2DStart()
	{
		m_PhysicsWorld = new b2World({ 0.0f, -19.8f });

		m_ContactListener = CreateScope<PhysicsContactListener>(this);
		m_PhysicsWorld->SetContactListener(m_ContactListener.get());

		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			b2BodyDef bodyDef;
			bodyDef.type = Rigidbody2DTypeToBox2DBody(rb2d.Type);
			bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
			bodyDef.angle = transform.Rotation.z;

			b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
			body->SetFixedRotation(rb2d.FixedRotation);
			body->GetUserData().pointer = static_cast<uintptr_t>(e);
			rb2d.RuntimeBody = body;

			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2PolygonShape boxShape;
				boxShape.SetAsBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = bc2d.Density;
				fixtureDef.friction = bc2d.Friction;
				fixtureDef.restitution = bc2d.Restitution;
				fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;
				body->CreateFixture(&fixtureDef);
			}

			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2CircleShape circleShape;
				circleShape.m_p.Set(cc2d.Offset.x, cc2d.Offset.y);
				circleShape.m_radius = transform.Scale.x * cc2d.Radius;

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &circleShape;
				fixtureDef.density = cc2d.Density;
				fixtureDef.friction = cc2d.Friction;
				fixtureDef.restitution = cc2d.Restitution;
				fixtureDef.restitutionThreshold = cc2d.RestitutionThreshold;
				body->CreateFixture(&fixtureDef);
			}
		}
	}

	void Scene::OnPhysics2DStop()
	{
		m_ContactListener.reset();
		delete m_PhysicsWorld;
		m_PhysicsWorld = nullptr;
	}

	void Scene::CreatePhysicsBody(Entity entity)
	{
		if (!m_PhysicsWorld)
			return;

		if (!entity.HasComponent<Rigidbody2DComponent>())
			return;

		auto& transform = entity.GetComponent<TransformComponent>();
		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

		if (rb2d.RuntimeBody)
			return; // Already has a physics body

		b2BodyDef bodyDef;
		bodyDef.type = Rigidbody2DTypeToBox2DBody(rb2d.Type);
		bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
		bodyDef.angle = transform.Rotation.z;

		b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
		body->SetFixedRotation(rb2d.FixedRotation);
		body->GetUserData().pointer = static_cast<uintptr_t>(static_cast<entt::entity>(entity));
		rb2d.RuntimeBody = body;

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

			b2PolygonShape boxShape;
			boxShape.SetAsBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &boxShape;
			fixtureDef.density = bc2d.Density;
			fixtureDef.friction = bc2d.Friction;
			fixtureDef.restitution = bc2d.Restitution;
			fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;
			body->CreateFixture(&fixtureDef);
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

			b2CircleShape circleShape;
			circleShape.m_p.Set(cc2d.Offset.x, cc2d.Offset.y);
			circleShape.m_radius = transform.Scale.x * cc2d.Radius;

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &circleShape;
			fixtureDef.density = cc2d.Density;
			fixtureDef.friction = cc2d.Friction;
			fixtureDef.restitution = cc2d.Restitution;
			fixtureDef.restitutionThreshold = cc2d.RestitutionThreshold;
			body->CreateFixture(&fixtureDef);
		}
	}

	void Scene::RecreatePhysicsBody(Entity entity)
	{
		if (!m_PhysicsWorld)
			return;

		if (!entity.HasComponent<Rigidbody2DComponent>())
			return;

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		if (rb2d.RuntimeBody)
		{
			m_PhysicsWorld->DestroyBody((b2Body*)rb2d.RuntimeBody);
			rb2d.RuntimeBody = nullptr;
		}

		CreatePhysicsBody(entity);
	}

	void Scene::RenderScene(EditorCamera& camera)
	{
		// Render 3D (static meshes) + 2D (sprites/circles) in one pipeline.
		SceneRenderer::BeginFrame(camera);

		SubmitSkybox();
		SubmitSceneLights();
		SubmitStaticMeshDraws();
		SubmitSkeletalMeshDraws();
		SubmitSpriteAndCircleDraws();
	}

	void Scene::RenderSceneFromCamera(const CameraComponent& cameraComp, const glm::mat4& cameraTransform)
	{
		// Render 3D (static meshes) + 2D (sprites/circles) in one pipeline.
		SceneRenderer::BeginFrame(cameraComp.Camera, cameraTransform);

		SubmitSkybox();
		SubmitSceneLights();
		SubmitStaticMeshDraws();
		SubmitSkeletalMeshDraws();
		SubmitSpriteAndCircleDraws();
	}

	void Scene::SubmitSkybox()
	{
		// First SkyboxComponent wins; a broken/empty path falls through to the
		// next one. The baked cubemap is cached per path by TextureCubemap, so
		// repeated lookups cost nothing after the first bake.
		auto view = m_Registry.view<SkyboxComponent>();
		for (auto entity : view)
		{
			auto& sb = view.get<SkyboxComponent>(entity);
			if (sb.CubemapPath.empty())
				continue;

			if (!sb.Cubemap)
			{
				sb.Cubemap = TextureCubemap::CreateFromEquirect(sb.CubemapPath);
				if (!sb.Cubemap)
				{
					CANDY_CORE_WARN("Scene::SubmitSkybox: failed to load cubemap '{}'", sb.CubemapPath);
					continue;
				}
			}

			SceneRenderer::SubmitSkybox(sb.Cubemap, sb.Intensity, sb.Exposure);
			return;
		}
	}

	void Scene::SubmitSceneLights()
	{
		SceneRenderer::SetAmbientLight(m_AmbientColor);

		auto view = m_Registry.view<TransformComponent, LightComponent>();
		for (auto entity : view)
		{
			auto [tc, light] = view.get<TransformComponent, LightComponent>(entity);

			SceneLight sceneLight;
			sceneLight.Type       = static_cast<int>(light.Type);
			sceneLight.Position   = tc.Translation;
			sceneLight.Color      = light.Color;
			sceneLight.Intensity  = light.Intensity;
			sceneLight.Range      = light.Range;
			// Directional/Spot shine along the entity's local -Z (camera
			// convention): the light travels from the transform's forward.
			sceneLight.Direction = glm::quat(tc.Rotation) * glm::vec3(0.0f, 0.0f, -1.0f);
			sceneLight.InnerConeCos = glm::cos(glm::radians(light.InnerConeAngle));
			sceneLight.OuterConeCos = glm::cos(glm::radians(light.OuterConeAngle));

			SceneRenderer::SubmitLight(sceneLight);
		}
	}

	void Scene::SubmitStaticMeshDraws()
	{
		auto view = m_Registry.view<TransformComponent, StaticMeshComponent>();
		for (auto entity : view)
		{
			auto [tc, smc] = view.get<TransformComponent, StaticMeshComponent>(entity);
			if (!smc.Mesh || smc.Mesh->Submeshes.empty())
				continue;

			const glm::mat4 transform = tc.GetTransform();
			for (size_t i = 0; i < smc.Mesh->Submeshes.size(); ++i)
			{
				MeshDrawCommand draw;
				draw.Transform     = transform;
				draw.Mesh          = smc.Mesh;
				draw.Material      = (i < smc.Materials.size()) ? smc.Materials[i] : nullptr;
				draw.SubmeshIndex  = static_cast<uint32_t>(i);
				draw.EntityID      = static_cast<int>(entity);
				SceneRenderer::Submit(draw);
			}
		}
	}

	void Scene::SubmitSkeletalMeshDraws()
	{
		auto view = m_Registry.view<TransformComponent, SkeletalMeshComponent>();
		for (auto entity : view)
		{
			auto [tc, smc] = view.get<TransformComponent, SkeletalMeshComponent>(entity);
			if (!smc.Mesh || smc.Mesh->Submeshes.empty() || smc.Mesh->Skeleton.empty())
				continue;

			// Per-instance bone CB written by SkeletalAnimationSystem::Update
			// each frame; shared by every submesh draw of this instance.
			const glm::mat4 transform = tc.GetTransform();
			Ref<RHIBuffer> boneBuffer = smc.BoneBuffer;
			for (size_t i = 0; i < smc.Mesh->Submeshes.size(); ++i)
			{
				MeshDrawCommand draw;
				draw.Transform     = transform;
				draw.SkinnedMesh   = smc.Mesh;
				draw.Material      = (i < smc.Materials.size()) ? smc.Materials[i] : nullptr;
				draw.SubmeshIndex  = static_cast<uint32_t>(i);
				draw.EntityID      = static_cast<int>(entity);
				draw.BoneMatrices  = boneBuffer;
				SceneRenderer::Submit(draw);
			}
		}
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		// static_assert(false);
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<LightComponent>(Entity entity, LightComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SkyboxComponent>(Entity entity, SkyboxComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<StaticMeshComponent>(Entity entity, StaticMeshComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SkeletalMeshComponent>(Entity entity, SkeletalMeshComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<AudioSourceComponent>(Entity entity, AudioSourceComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<UITextBlockComponent>(Entity entity, UITextBlockComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<UIButtonComponent>(Entity entity, UIButtonComponent& component)
	{
	}
}