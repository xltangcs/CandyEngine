#include "CandyPCH.h"

#include "Runtime/Renderer/SceneDrawCollector.h"
#include "Runtime/Renderer/SceneRenderer.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Renderer/Texture.h"
#include "Runtime/Renderer/TextureCubemap.h"
#include "Runtime/Asset/StaticMeshResource.h"
#include "Runtime/Asset/SkeletalMeshResource.h"
#include "Runtime/Asset/Material.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Core/Log.h"

#include <glm/gtc/quaternion.hpp>

namespace Candy {

	namespace {

		// Shared built-in rendering resources (sprite/circle migration) — one
		// instance per process, shared by every scene (read-only data).
		Ref<StaticMeshResource> s_QuadMesh;
		Ref<Material> s_SpriteMaterial;
		Ref<Material> s_CircleMaterial;

	}

	const Ref<StaticMeshResource>& SceneDrawCollector::GetSpriteQuad()
	{
		if (!s_QuadMesh)
			s_QuadMesh = StaticMeshResource::CreateQuad();
		return s_QuadMesh;
	}

	const Ref<Material>& SceneDrawCollector::GetSpriteMaterial()
	{
		if (!s_SpriteMaterial)
		{
			s_SpriteMaterial = CreateRef<Material>();
			s_SpriteMaterial->Name = "Builtin/Sprite";
			s_SpriteMaterial->ShaderPath = "VFS://Engine/Content/Shaders/D3D12/Sprite.hlsl";
			s_SpriteMaterial->ShaderParams["u_BlendMode"] = 2.0f; // Transparent
		}
		return s_SpriteMaterial;
	}

	const Ref<Material>& SceneDrawCollector::GetCircleMaterial()
	{
		if (!s_CircleMaterial)
		{
			s_CircleMaterial = CreateRef<Material>();
			s_CircleMaterial->Name = "Builtin/Circle";
			s_CircleMaterial->ShaderPath = "VFS://Engine/Content/Shaders/D3D12/Sprite.hlsl";
			s_CircleMaterial->ShaderParams["u_BlendMode"]  = 2.0f; // Transparent
			s_CircleMaterial->ShaderParams["u_CircleMode"] = 1.0f; // SDF circle
		}
		return s_CircleMaterial;
	}

	void SceneDrawCollector::SubmitScene(Scene& scene, EditorCamera& camera)
	{
		SceneRenderer::BeginFrame(camera);

		SubmitSkybox(scene);
		SubmitSceneLights(scene);
		SubmitStaticMeshDraws(scene);
		SubmitSkeletalMeshDraws(scene);
		SubmitSpriteAndCircleDraws(scene);
	}

	void SceneDrawCollector::SubmitScene(Scene& scene, const CameraComponent& cameraComp, const glm::mat4& cameraTransform)
	{
		SceneRenderer::BeginFrame(cameraComp.Camera, cameraTransform);

		SubmitSkybox(scene);
		SubmitSceneLights(scene);
		SubmitStaticMeshDraws(scene);
		SubmitSkeletalMeshDraws(scene);
		SubmitSpriteAndCircleDraws(scene);
	}

	void SceneDrawCollector::SubmitRuntimeScene(Scene& scene)
	{
		// Render 2D (sprites/circles) + 3D via the unified scene renderer.
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		{
			auto view = scene.GetRegistry().view<TransformComponent, CameraComponent>();
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
			mainCamera = &scene.GetFallbackCamera();
			cameraTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			scene.GetFallbackCamera().SetViewportSize(scene.GetViewportWidth(), scene.GetViewportHeight());
		}

		SceneRenderer::BeginFrame(*mainCamera, cameraTransform);
		SubmitSkybox(scene);
		SubmitSceneLights(scene);
		SubmitStaticMeshDraws(scene);
		SubmitSkeletalMeshDraws(scene);
		SubmitSpriteAndCircleDraws(scene);
	}

	void SceneDrawCollector::SubmitSpriteAndCircleDraws(Scene& scene)
	{
		const auto& quad = GetSpriteQuad();
		auto& registry = scene.GetRegistry();

		// Sprites
		{
			auto group = registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
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
			auto view = registry.view<TransformComponent, CircleRendererComponent>();
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

	void SceneDrawCollector::SubmitSkybox(Scene& scene)
	{
		// First SkyboxComponent wins; a broken/empty path falls through to the
		// next one. The baked cubemap is cached per path by TextureCubemap, so
		// repeated lookups cost nothing after the first bake.
		auto view = scene.GetRegistry().view<SkyboxComponent>();
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
					CANDY_CORE_WARN("SceneDrawCollector: failed to load cubemap '{}'", sb.CubemapPath);
					continue;
				}
			}

			SceneRenderer::SubmitSkybox(sb.Cubemap, sb.Intensity, sb.Exposure);
			return;
		}
	}

	void SceneDrawCollector::SubmitSceneLights(Scene& scene)
	{
		SceneRenderer::SetAmbientLight(scene.GetAmbientLight());

		auto view = scene.GetRegistry().view<TransformComponent, LightComponent>();
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

	void SceneDrawCollector::SubmitStaticMeshDraws(Scene& scene)
	{
		auto view = scene.GetRegistry().view<TransformComponent, StaticMeshComponent>();
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

	void SceneDrawCollector::SubmitSkeletalMeshDraws(Scene& scene)
	{
		auto view = scene.GetRegistry().view<TransformComponent, SkeletalMeshComponent>();
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

} // namespace Candy
