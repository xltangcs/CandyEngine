#pragma once

#include "Runtime/Scene/SceneCamera.h"
#include "Runtime/Core/UUID.h"
#include "Runtime/Renderer/Texture.h"
#include "Runtime/Renderer/TextureCubemap.h"
#include "Runtime/RHI/RHIDevice.h"
#include "Runtime/Asset/StaticMeshResource.h"
#include "Runtime/Asset/SkeletalMeshResource.h"
#include "Runtime/Asset/Material.h"


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <unordered_map>
#include <vector>

#include "Runtime/Scripting/ScriptBindingsMacros.h"

namespace Candy {

	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(const UUID& id) : ID(id) {} 
	};

	CANDY_CLASS()
	struct TagComponent
	{
		CANDY_PROPERTY()
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};
	CANDY_CLASS()
	struct TransformComponent
	{
		CANDY_PROPERTY()
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		CANDY_PROPERTY()
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		CANDY_PROPERTY()
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	CANDY_CLASS()
	struct SpriteRendererComponent
	{
		CANDY_PROPERTY()
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		CANDY_PROPERTY()
		std::string TexturePath;     // VFS:// format, empty = no texture (serialized for save/load)
		CANDY_PROPERTY()
		float TilingFactor = 1.0f;
		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {}
	};

	CANDY_CLASS()
	struct StaticMeshComponent
	{
		// VFS:// path to the source glTF asset (serialized for save/load).
		CANDY_PROPERTY()
		std::string MeshPath;

		// One .mat VFS path per Submesh (indexed by Submesh::MaterialIndex).
		// Serialized for save/load; each entry points at a shared material asset.
		// May be empty (meaning "use the mesh's imported default material").
		CANDY_PROPERTY()
		std::vector<std::string> MaterialPaths;

		// Runtime imported data (not serialized). Populated from MeshPath via
		// MeshImporter when the scene is loaded / the path changes, then refined
		// by MaterialCache so each MaterialPaths[i] resolves to a shared material.
		Ref<StaticMeshResource> Mesh;
		// One material per Submesh (indexed by Submesh::MaterialIndex).
		// Shared via MaterialCache when MaterialPaths[i] is set.
		std::vector<Ref<Material>> Materials;

		StaticMeshComponent() = default;
		StaticMeshComponent(const StaticMeshComponent&) = default;
	};

	CANDY_CLASS()
	struct SkeletalMeshComponent
	{
		// VFS:// path to the source skinned glTF asset (serialized for save/load).
		CANDY_PROPERTY()
		std::string MeshPath;

		// One .mat VFS path per Submesh (indexed by Submesh::MaterialIndex).
		CANDY_PROPERTY()
		std::vector<std::string> MaterialPaths;

		// Animation playback state (serialized).
		CANDY_PROPERTY()
		std::string ClipName;
		CANDY_PROPERTY()
		float Speed = 1.0f;
		CANDY_PROPERTY()
		bool Play = true;
		CANDY_PROPERTY()
		bool Loop = true;

		// Runtime imported data (not serialized).
		Ref<SkeletalMeshResource> Mesh;
		std::vector<Ref<Material>> Materials;

		// Current animation time in seconds (runtime; advanced by
		// SkeletalAnimationSystem, seekable from the inspector).
		float Time = 0.0f;

		// Editor-only: draw the joint hierarchy as debug lines (RenderOverlay).
		bool ShowSkeleton = false;

		// Most recent global joint poses (mesh-space skin matrices not applied),
		// filled by SkeletalAnimationSystem for editor skeleton debug lines.
		std::vector<glm::mat4> DebugGlobalPose;

		// Per-instance bone CB (runtime; owned here so each instance evaluates
		// its own pose — never shared across entities or scene copies).
		Ref<RHIBuffer> BoneBuffer;
		uint32_t BoneBufferJoints = 0;

		SkeletalMeshComponent() = default;
		SkeletalMeshComponent(const SkeletalMeshComponent&) = default;
	};

	CANDY_CLASS()
	struct CircleRendererComponent
	{
		CANDY_PROPERTY()
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		CANDY_PROPERTY()
		float Thickness = 1.0f;
		CANDY_PROPERTY()
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	struct CameraComponent
	{
		SceneCamera Camera;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	CANDY_CLASS()
	struct LightComponent
	{
		CANDY_ENUM()
		enum class LightType { Directional = 0, Point, Spot };

		CANDY_PROPERTY()
		LightType Type = LightType::Directional;
		CANDY_PROPERTY()
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		CANDY_PROPERTY()
		float Intensity = 1.0f;
		CANDY_PROPERTY()
		float Range = 10.0f;           // Point / Spot
		CANDY_PROPERTY()
		float InnerConeAngle = 12.5f;  // Spot, degrees
		CANDY_PROPERTY()
		float OuterConeAngle = 45.0f;  // Spot, degrees
		CANDY_PROPERTY()
		bool CastShadows = false;      // reserved, not implemented yet

		LightComponent() = default;
		LightComponent(const LightComponent&) = default;
	};

	CANDY_CLASS()
	struct SkyboxComponent
	{
		/// VFS:// path to an equirectangular panorama (.hdr or LDR image).
		/// Converted + CPU-baked into a cubemap + IBL set on load (cached).
		CANDY_PROPERTY()
		std::string CubemapPath;

		/// Multiplier on the IBL (irradiance + specular) contribution.
		CANDY_PROPERTY()
		float Intensity = 1.0f;

		/// Global scene exposure (tonemap pass). Also scales the skybox
		/// itself. 1.0 = neutral.
		CANDY_PROPERTY()
		float Exposure = 1.0f;

		/// Runtime baked cubemap (not serialized; re-created from CubemapPath).
		Ref<TextureCubemap> Cubemap;

		SkyboxComponent() = default;
		SkyboxComponent(const SkyboxComponent&) = default;
	};

	// Forward declaration
	class ScriptableEntity;

	struct NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		template<typename T>
		void Bind()
		{
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	// Physics

	// Rigidbody2DComponent bound manually in PythonBindings.cpp (custom physics methods)
	struct Rigidbody2DComponent
	{
		CANDY_ENUM()
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;
		bool FixedRotation = false;

		// Storage for runtime
		void* RuntimeBody = nullptr;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	CANDY_CLASS()
	struct BoxCollider2DComponent
	{
		CANDY_PROPERTY()
		glm::vec2 Offset = { 0.0f, 0.0f };
		CANDY_PROPERTY()
		glm::vec2 Size = { 0.5f, 0.5f };

		CANDY_PROPERTY()
		float Density = 1.0f;
		CANDY_PROPERTY()
		float Friction = 0.5f;
		CANDY_PROPERTY()
		float Restitution = 0.0f;
		CANDY_PROPERTY()
		float RestitutionThreshold = 0.5f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	CANDY_CLASS()
	struct CircleCollider2DComponent
	{
		CANDY_PROPERTY()
		glm::vec2 Offset = { 0.0f, 0.0f };
		CANDY_PROPERTY()
		float Radius = 0.5f;

		CANDY_PROPERTY()
		float Density = 1.0f;
		CANDY_PROPERTY()
		float Friction = 0.5f;
		CANDY_PROPERTY()
		float Restitution = 0.0f;
		CANDY_PROPERTY()
		float RestitutionThreshold = 0.5f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	CANDY_CLASS()
	struct ScriptComponent
	{
		CANDY_PROPERTY()
		std::string ScriptPath;
		CANDY_PROPERTY()
		std::string ClassName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
	};

	// Audio

	CANDY_CLASS()
	struct AudioSourceComponent
	{
		CANDY_PROPERTY()
		std::string SoundPath;
		CANDY_PROPERTY()
		float Volume = 1.0f;
		CANDY_PROPERTY()
		bool Looping = false;
		CANDY_PROPERTY()
		bool PlayOnStart = false;

		void* RuntimeHandle = nullptr;  // ma_sound* at runtime

		AudioSourceComponent() = default;
		AudioSourceComponent(const AudioSourceComponent&) = default;
	};

	// UI

	CANDY_CLASS()
	struct TextBlockUIData
	{
		CANDY_PROPERTY()
		std::string Text;
		CANDY_PROPERTY()
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		CANDY_PROPERTY()
		glm::vec2 Position = { 0.0f, 0.0f };
		CANDY_PROPERTY()
		float FontSize = 24.0f;
		CANDY_PROPERTY()
		bool Visible = true;
	};

	// UITextBlockComponent bound manually in PythonBindings.cpp (custom set_text/set_text_visible methods)
	struct UITextBlockComponent
	{
		std::unordered_map<std::string, TextBlockUIData> TextBlockDatas;
		std::vector<std::string> TextBlockOrder;

		UITextBlockComponent() = default;
		UITextBlockComponent(const UITextBlockComponent&) = default;
	};

	CANDY_CLASS()
	struct ButtonUIData
	{
		CANDY_PROPERTY()
		std::string Text;
		CANDY_PROPERTY()
		float FontSize = 24.0f;
		CANDY_PROPERTY()
		glm::vec2 Size = { 200.0f, 50.0f };
		CANDY_PROPERTY()
		glm::vec2 Position = { 0.0f, 0.0f };
		CANDY_PROPERTY()
		std::string OnClick;
		CANDY_PROPERTY()
		bool Visible = true;
	};

	// UIButtonComponent bound manually in PythonBindings.cpp (custom set_button_*/get_button_* methods)
	struct UIButtonComponent
	{
		std::unordered_map<std::string, ButtonUIData> ButtonDatas;
		std::vector<std::string> ButtonOrder;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
	};
}
