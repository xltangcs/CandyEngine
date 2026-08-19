#include "CandyPCH.h"

#include "Runtime/Scene/Entity.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Scene/SceneSerializer.h"
#include "Runtime/Core/VfsPath.h"
#include "Runtime/Renderer/Texture.h"
#include "Runtime/Renderer/TextureCubemap.h"
#include "Runtime/Asset/MeshImporter.h"
#include "Runtime/Asset/MaterialCache.h"

#include "Runtime/Scene/SceneSerialization.generated.inl"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace YAML {

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

}
namespace Candy {

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	static std::string RigidBody2DBodyTypeToString(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
		case Rigidbody2DComponent::BodyType::Static:    return "Static";
		case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
		case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
		}

		CANDY_CORE_ASSERT(false, "Unknown body type");
		return {};
	}

	static Rigidbody2DComponent::BodyType RigidBody2DBodyTypeFromString(const std::string& bodyTypeString)
	{
		if (bodyTypeString == "Static")    return Rigidbody2DComponent::BodyType::Static;
		if (bodyTypeString == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
		if (bodyTypeString == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;

		CANDY_CORE_ASSERT(false, "Unknown body type");
		return Rigidbody2DComponent::BodyType::Static;
	}

	static std::string LightTypeToString(LightComponent::LightType type)
	{
		switch (type)
		{
		case LightComponent::LightType::Directional: return "Directional";
		case LightComponent::LightType::Point:       return "Point";
		case LightComponent::LightType::Spot:        return "Spot";
		}

		CANDY_CORE_ASSERT(false, "Unknown light type");
		return {};
	}

	static LightComponent::LightType LightTypeFromString(const std::string& typeString)
	{
		if (typeString == "Directional") return LightComponent::LightType::Directional;
		if (typeString == "Point")       return LightComponent::LightType::Point;
		if (typeString == "Spot")        return LightComponent::LightType::Spot;

		CANDY_CORE_ASSERT(false, "Unknown light type");
		return LightComponent::LightType::Directional;
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		CANDY_CORE_ASSERT(entity.HasComponent<IDComponent>());

		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID(); // TODO: Entity ID goes here

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			GeneratedSerialization::SerializeTransformComponent(out, entity.GetComponent<TransformComponent>());

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<LightComponent>())
		{
			out << YAML::Key << "LightComponent";
			out << YAML::BeginMap; // LightComponent

			auto& lc = entity.GetComponent<LightComponent>();
			out << YAML::Key << "Type" << YAML::Value << LightTypeToString(lc.Type);
			out << YAML::Key << "Color" << YAML::Value << lc.Color;
			out << YAML::Key << "Intensity" << YAML::Value << lc.Intensity;
			out << YAML::Key << "Range" << YAML::Value << lc.Range;
			out << YAML::Key << "InnerConeAngle" << YAML::Value << lc.InnerConeAngle;
			out << YAML::Key << "OuterConeAngle" << YAML::Value << lc.OuterConeAngle;
			out << YAML::Key << "CastShadows" << YAML::Value << lc.CastShadows;

			out << YAML::EndMap; // LightComponent
		}

		if (entity.HasComponent<SkyboxComponent>())
		{
			out << YAML::Key << "SkyboxComponent";
			out << YAML::BeginMap; // SkyboxComponent

			GeneratedSerialization::SerializeSkyboxComponent(out, entity.GetComponent<SkyboxComponent>());

			out << YAML::EndMap; // SkyboxComponent
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent

			GeneratedSerialization::SerializeSpriteRendererComponent(out, entity.GetComponent<SpriteRendererComponent>());

			out << YAML::EndMap; // SpriteRendererComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; // StaticMeshComponent

			GeneratedSerialization::SerializeStaticMeshComponent(out, entity.GetComponent<StaticMeshComponent>());

			out << YAML::EndMap; // StaticMeshComponent
		}

		if (entity.HasComponent<SkeletalMeshComponent>())
		{
			out << YAML::Key << "SkeletalMeshComponent";
			out << YAML::BeginMap; // SkeletalMeshComponent

			GeneratedSerialization::SerializeSkeletalMeshComponent(out, entity.GetComponent<SkeletalMeshComponent>());

			out << YAML::EndMap; // SkeletalMeshComponent
		}

		if (entity.HasComponent<CircleRendererComponent>())
		{
			out << YAML::Key << "CircleRendererComponent";
			out << YAML::BeginMap; // CircleRendererComponent

			auto& circleRendererComponent = entity.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << circleRendererComponent.Color;
			out << YAML::Key << "Thickness" << YAML::Value << circleRendererComponent.Thickness;
			out << YAML::Key << "Fade" << YAML::Value << circleRendererComponent.Fade;

			out << YAML::EndMap; // CircleRendererComponent
		}

		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap; // Rigidbody2DComponent

			auto& rb2dComponent = entity.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << RigidBody2DBodyTypeToString(rb2dComponent.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << rb2dComponent.FixedRotation;

			out << YAML::EndMap; // Rigidbody2DComponent
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap; // BoxCollider2DComponent

			GeneratedSerialization::SerializeBoxCollider2DComponent(out, entity.GetComponent<BoxCollider2DComponent>());

			out << YAML::EndMap; // BoxCollider2DComponent
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap; // CircleCollider2DComponent

			GeneratedSerialization::SerializeCircleCollider2DComponent(out, entity.GetComponent<CircleCollider2DComponent>());

			out << YAML::EndMap; // CircleCollider2DComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent

			GeneratedSerialization::SerializeScriptComponent(out, entity.GetComponent<ScriptComponent>());

			out << YAML::EndMap; // ScriptComponent
		}

		if (entity.HasComponent<AudioSourceComponent>())
		{
			out << YAML::Key << "AudioSourceComponent";
			out << YAML::BeginMap; // AudioSourceComponent

			GeneratedSerialization::SerializeAudioSourceComponent(out, entity.GetComponent<AudioSourceComponent>());

			out << YAML::EndMap; // AudioSourceComponent
		}

		if (entity.HasComponent<UITextBlockComponent>())
		{
			out << YAML::Key << "UITextBlockComponent";
			out << YAML::BeginMap; // UITextBlockComponent

			auto& ui = entity.GetComponent<UITextBlockComponent>();
			out << YAML::Key << "TextBlocks";
			out << YAML::BeginMap; // TextBlocks
			for (auto& key : ui.TextBlockOrder)
			{
				auto& tb = ui.TextBlockDatas[key];
				out << YAML::Key << key;
				out << YAML::BeginMap; // TextBlock
				out << YAML::Key << "Text" << YAML::Value << tb.Text;
				out << YAML::Key << "Color" << YAML::Value << tb.Color;
				out << YAML::Key << "Position" << YAML::Value << tb.Position;
				out << YAML::Key << "FontSize" << YAML::Value << tb.FontSize;
				out << YAML::Key << "Visible" << YAML::Value << tb.Visible;
				out << YAML::EndMap; // TextBlock
			}
			out << YAML::EndMap; // TextBlocks

			out << YAML::EndMap; // UITextBlockComponent
		}

		if (entity.HasComponent<UIButtonComponent>())
		{
			out << YAML::Key << "UIButtonComponent";
			out << YAML::BeginMap; // UIButtonComponent

			auto& ui = entity.GetComponent<UIButtonComponent>();
			out << YAML::Key << "Buttons";
			out << YAML::BeginMap; // Buttons
			for (auto& key : ui.ButtonOrder)
			{
				auto& btn = ui.ButtonDatas[key];
				out << YAML::Key << key;
				out << YAML::BeginMap; // Button
				out << YAML::Key << "Text" << YAML::Value << btn.Text;
				out << YAML::Key << "FontSize" << YAML::Value << btn.FontSize;
				out << YAML::Key << "Size" << YAML::Value << btn.Size;
				out << YAML::Key << "Position" << YAML::Value << btn.Position;
				out << YAML::Key << "OnClick" << YAML::Value << btn.OnClick;
				out << YAML::Key << "Visible" << YAML::Value << btn.Visible;
				out << YAML::EndMap; // Button
			}
			out << YAML::EndMap; // Buttons

			out << YAML::EndMap; // UIButtonComponent
		}

		out << YAML::EndMap; // Entity

	}

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		std::string::size_type iPos = filepath.find_last_of('\\') + 1;
		std::string filename = filepath.substr(iPos, filepath.length() - iPos);
		filename = filename.substr(0, filename.rfind("."));

		CANDY_CORE_INFO("Serialize Path is {0}", filepath);
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << filename;
		out << YAML::Key << "AmbientColor" << YAML::Value << m_Scene->m_AmbientColor;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		for (const auto [entityID] : m_Scene->m_Registry.storage<entt::entity>().reach())
		{
			Entity entity = { entityID, m_Scene.get() };
			if (!entity)
				return;

			SerializeEntity(out, entity);
		}

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	void SceneSerializer::SerializeRuntime(const std::string& filepath)
	{
		// Not implemented
		CANDY_CORE_ASSERT(false);
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		CANDY_CORE_INFO("Deserialize Path is {0}", filepath);

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath);
		}
		catch (YAML::ParserException e)
		{
			return false;
		}

		return DeserializeNode(data);
	}

	bool SceneSerializer::DeserializeFromString(const std::string& yamlContent)
	{
		YAML::Node data;
		try
		{
			data = YAML::Load(yamlContent);
		}
		catch (YAML::ParserException e)
		{
			return false;
		}

		return DeserializeNode(data);
	}

	bool SceneSerializer::DeserializeNode(YAML::Node& data)
	{
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		CANDY_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		if (auto ambient = data["AmbientColor"])
			m_Scene->m_AmbientColor = ambient.as<glm::vec3>();

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();

				CANDY_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);

				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);
				auto transformComponent = entity["TransformComponent"];
				if (transformComponent)
				{
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					GeneratedSerialization::DeserializeTransformComponent(transformComponent, tc);
				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					auto cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());

					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());

					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}

				auto lightComponent = entity["LightComponent"];
				if (lightComponent)
				{
					auto& lc = deserializedEntity.AddComponent<LightComponent>();
					lc.Type = LightTypeFromString(lightComponent["Type"].as<std::string>());
					lc.Color = lightComponent["Color"].as<glm::vec3>();
					lc.Intensity = lightComponent["Intensity"].as<float>();
					lc.Range = lightComponent["Range"].as<float>();
					lc.InnerConeAngle = lightComponent["InnerConeAngle"].as<float>();
					lc.OuterConeAngle = lightComponent["OuterConeAngle"].as<float>();
					lc.CastShadows = lightComponent["CastShadows"].as<bool>();
				}

				auto skyboxComponent = entity["SkyboxComponent"];
				if (skyboxComponent)
				{
					auto& sb = deserializedEntity.AddComponent<SkyboxComponent>();
					GeneratedSerialization::DeserializeSkyboxComponent(skyboxComponent, sb);
					if (!sb.CubemapPath.empty())
					{
						sb.Cubemap = TextureCubemap::CreateFromEquirect(sb.CubemapPath);
						if (!sb.Cubemap)
							CANDY_CORE_WARN("SceneSerializer: failed to load cubemap {0}", sb.CubemapPath);
					}
				}

				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					GeneratedSerialization::DeserializeSpriteRendererComponent(spriteRendererComponent, src);
					if (!src.TexturePath.empty())
					{
						auto tex = Texture2D::Create(src.TexturePath); // sprite maps are color maps -> default sRGB
						if (tex && tex->IsLoaded())
							src.Texture = tex;
						else
							CANDY_CORE_WARN("SceneSerializer: failed to load texture {0}", src.TexturePath);
					}
				}

				auto staticMeshComponent = entity["StaticMeshComponent"];
				if (staticMeshComponent)
				{
					auto& smc = deserializedEntity.AddComponent<StaticMeshComponent>();
					GeneratedSerialization::DeserializeStaticMeshComponent(staticMeshComponent, smc);
					if (!smc.MeshPath.empty())
					{
						auto imported = MeshImporter::ImportStaticMesh(smc.MeshPath);
						if (imported && imported->Mesh)
						{
							smc.Mesh = imported->Mesh;
							smc.Materials = imported->Materials;
						}
						else
							CANDY_CORE_WARN("SceneSerializer: failed to import mesh {0}", smc.MeshPath);
					}

					// Material overrides: resolve each .mat path through the cache
					// so a mesh that referenced a shared .mat stays shared after load.
					if (!smc.MaterialPaths.empty())
					{
						// Ensure the materials list has at least as many slots as
						// overrides, then replace each slot with the cached asset.
						if (smc.Materials.size() < smc.MaterialPaths.size())
							smc.Materials.resize(smc.MaterialPaths.size());
						for (size_t i = 0; i < smc.MaterialPaths.size(); i++)
						{
							auto mat = MaterialCache::Get().Load(smc.MaterialPaths[i]);
							if (mat)
								smc.Materials[i] = mat;
						}
					}
				}

				auto skeletalMeshComponent = entity["SkeletalMeshComponent"];
				if (skeletalMeshComponent)
				{
					auto& skmc = deserializedEntity.AddComponent<SkeletalMeshComponent>();
					GeneratedSerialization::DeserializeSkeletalMeshComponent(skeletalMeshComponent, skmc);
					if (!skmc.MeshPath.empty())
					{
						auto imported = MeshImporter::ImportSkeletalMesh(skmc.MeshPath);
						if (imported && imported->Mesh)
						{
							skmc.Mesh = imported->Mesh;
							skmc.Materials = imported->Materials;
							if (skmc.ClipName.empty() && !imported->Mesh->Skeleton->Clips.empty())
								skmc.ClipName = imported->Mesh->Skeleton->Clips[0].Name;
						}
						else
							CANDY_CORE_WARN("SceneSerializer: failed to import skeletal mesh {0}", skmc.MeshPath);
					}

					if (!skmc.MaterialPaths.empty())
					{
						if (skmc.Materials.size() < skmc.MaterialPaths.size())
							skmc.Materials.resize(skmc.MaterialPaths.size());
						for (size_t i = 0; i < skmc.MaterialPaths.size(); i++)
						{
							auto mat = MaterialCache::Get().Load(skmc.MaterialPaths[i]);
							if (mat)
								skmc.Materials[i] = mat;
						}
					}
				}

				auto circleRendererComponent = entity["CircleRendererComponent"];
				if (circleRendererComponent)
				{
					auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
					crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
					crc.Thickness = circleRendererComponent["Thickness"].as<float>();
					crc.Fade = circleRendererComponent["Fade"].as<float>();
				}

				auto rigidbody2DComponent = entity["Rigidbody2DComponent"];
				if (rigidbody2DComponent)
				{
					auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
					rb2d.Type = RigidBody2DBodyTypeFromString(rigidbody2DComponent["BodyType"].as<std::string>());
					rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();
				}

				auto boxCollider2DComponent = entity["BoxCollider2DComponent"];
				if (boxCollider2DComponent)
				{
					auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
					GeneratedSerialization::DeserializeBoxCollider2DComponent(boxCollider2DComponent, bc2d);
				}

				auto circleCollider2DComponent = entity["CircleCollider2DComponent"];
				if (circleCollider2DComponent)
				{
					auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
					GeneratedSerialization::DeserializeCircleCollider2DComponent(circleCollider2DComponent, cc2d);
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					GeneratedSerialization::DeserializeScriptComponent(scriptComponent, sc);
				}

				auto audioSourceComponent = entity["AudioSourceComponent"];
				if (audioSourceComponent)
				{
					auto& asc = deserializedEntity.AddComponent<AudioSourceComponent>();
					GeneratedSerialization::DeserializeAudioSourceComponent(audioSourceComponent, asc);
				}

				auto uiTextBlockComponent = entity["UITextBlockComponent"];
				if (uiTextBlockComponent)
				{
					auto& ui = deserializedEntity.AddComponent<UITextBlockComponent>();
					auto textBlocks = uiTextBlockComponent["TextBlocks"];
					if (textBlocks)
					{
						for (auto tbNode : textBlocks)
						{
							auto key = tbNode.first.as<std::string>();
							auto& tbData = tbNode.second;
							TextBlockUIData tb;
							tb.Text = tbData["Text"].as<std::string>();
							tb.Color = tbData["Color"].as<glm::vec4>();
							tb.Position = tbData["Position"].as<glm::vec2>();
							tb.FontSize = tbData["FontSize"].as<float>();
							tb.Visible = tbData["Visible"].as<bool>();
							ui.TextBlockDatas[key] = tb;
						ui.TextBlockOrder.push_back(key);
						}
					}
				}

				auto uiButtonComponent = entity["UIButtonComponent"];
				if (uiButtonComponent)
				{
					auto& ui = deserializedEntity.AddComponent<UIButtonComponent>();
					auto buttons = uiButtonComponent["Buttons"];
					if (buttons)
					{
						for (auto btnNode : buttons)
						{
							auto key = btnNode.first.as<std::string>();
							auto& btnData = btnNode.second;
							ButtonUIData btn;
							btn.Text = btnData["Text"].as<std::string>();
							btn.FontSize = btnData["FontSize"].as<float>();
							btn.Size = btnData["Size"].as<glm::vec2>();
							btn.Position = btnData["Position"].as<glm::vec2>();
							btn.OnClick = btnData["OnClick"].as<std::string>();
							btn.Visible = btnData["Visible"].as<bool>();
							ui.ButtonDatas[key] = btn;
						ui.ButtonOrder.push_back(key);
						}
					}
				}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
	{
		// Not implemented
		CANDY_CORE_ASSERT(false);
		return false;
	}

}