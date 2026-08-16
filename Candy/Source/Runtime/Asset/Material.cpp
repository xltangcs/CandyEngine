#include "CandyPCH.h"

#include "Runtime/Asset/Material.h"
#include "Runtime/Core/FileSystem.h"

#include <yaml-cpp/yaml.h>

namespace Candy {

	namespace Utils {

		static const char* BlendModeToString(MaterialBlendMode mode)
		{
			switch (mode)
			{
				case MaterialBlendMode::Opaque:      return "Opaque";
				case MaterialBlendMode::Masked:      return "Masked";
				case MaterialBlendMode::Transparent: return "Transparent";
				default:                             return "Opaque";
			}
		}

		static MaterialBlendMode BlendModeFromString(const std::string& str)
		{
			if (str == "Masked")      return MaterialBlendMode::Masked;
			if (str == "Transparent") return MaterialBlendMode::Transparent;
			return MaterialBlendMode::Opaque;
		}

		static const char* ShadingModelToString(MaterialShadingModel model)
		{
			switch (model)
			{
				case MaterialShadingModel::Lit:   return "Lit";
				case MaterialShadingModel::Unlit: return "Unlit";
				default:                          return "Lit";
			}
		}

		static MaterialShadingModel ShadingModelFromString(const std::string& str)
		{
			if (str == "Unlit") return MaterialShadingModel::Unlit;
			return MaterialShadingModel::Lit;
		}

		// --- glm helper emission (local to this TU to stay independent of the
		// scene serializer's own operators) ------------------------------------

		static void EmitVec2(YAML::Emitter& out, const glm::vec2& v)
		{
			out << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		}

		static void EmitVec3(YAML::Emitter& out, const glm::vec3& v)
		{
			out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		}

		static bool ReadVec3(const YAML::Node& node, glm::vec3& out)
		{
			if (!node || !node.IsSequence() || node.size() != 3)
				return false;
			out = glm::vec3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>());
			return true;
		}

		static bool ReadVec2(const YAML::Node& node, glm::vec2& out)
		{
			if (!node || !node.IsSequence() || node.size() != 2)
				return false;
			out = glm::vec2(node[0].as<float>(), node[1].as<float>());
			return true;
		}

	} // namespace Utils

	bool Material::Serialize(const std::string& virtualPath) const
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value;
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Name" << YAML::Value << Name;
			out << YAML::Key << "BaseColor" << YAML::Value;
			Utils::EmitVec3(out, BaseColor);
			out << YAML::Key << "Metallic"  << YAML::Value << Metallic;
			out << YAML::Key << "Roughness" << YAML::Value << Roughness;

			out << YAML::Key << "EmissiveColor" << YAML::Value;
			Utils::EmitVec3(out, EmissiveColor);
			out << YAML::Key << "EmissiveIntensity" << YAML::Value << EmissiveIntensity;

			out << YAML::Key << "BlendMode"    << YAML::Value << Utils::BlendModeToString(BlendMode);
			out << YAML::Key << "ShadingModel" << YAML::Value << Utils::ShadingModelToString(ShadingModel);

			out << YAML::Key << "BaseColorMap"         << YAML::Value << BaseColorMap;
			out << YAML::Key << "NormalMap"            << YAML::Value << NormalMap;
			out << YAML::Key << "MetallicRoughnessMap" << YAML::Value << MetallicRoughnessMap;
			out << YAML::Key << "EmissiveMap"          << YAML::Value << EmissiveMap;

			out << YAML::Key << "UVTiling" << YAML::Value;
			Utils::EmitVec2(out, UVTiling);
			out << YAML::Key << "UVOffset" << YAML::Value;
			Utils::EmitVec2(out, UVOffset);

			out << YAML::EndMap;
		}
		out << YAML::EndMap;

		return FileSystem::Get().WriteText(virtualPath, std::string(out.c_str()));
	}

	Ref<Material> Material::Load(const std::string& virtualPath)
	{
		std::optional<std::string> text = FileSystem::Get().ReadText(virtualPath);
		if (!text)
		{
			CANDY_CORE_ERROR("Material::Load - failed to read material file: {}", virtualPath);
			return nullptr;
		}

		YAML::Node data;
		try
		{
			data = YAML::Load(*text);
		}
		catch (const std::exception& e)
		{
			CANDY_CORE_ERROR("Material::Load - failed to parse YAML in {}: {}", virtualPath, e.what());
			return nullptr;
		}

		if (!data["Material"])
		{
			CANDY_CORE_ERROR("Material::Load - missing 'Material' root node in {}", virtualPath);
			return nullptr;
		}

		Ref<Material> material = CreateRef<Material>();
		const YAML::Node& root = data["Material"];

		glm::vec3 v3;
		glm::vec2 v2;

		if (root["Name"])
			material->Name = root["Name"].as<std::string>();
		if (Utils::ReadVec3(root["BaseColor"], v3))
			material->BaseColor = v3;
		if (root["Metallic"])
			material->Metallic = root["Metallic"].as<float>();
		if (root["Roughness"])
			material->Roughness = root["Roughness"].as<float>();

		if (Utils::ReadVec3(root["EmissiveColor"], v3))
			material->EmissiveColor = v3;
		if (root["EmissiveIntensity"])
			material->EmissiveIntensity = root["EmissiveIntensity"].as<float>();

		if (root["BlendMode"])
			material->BlendMode = Utils::BlendModeFromString(root["BlendMode"].as<std::string>());
		if (root["ShadingModel"])
			material->ShadingModel = Utils::ShadingModelFromString(root["ShadingModel"].as<std::string>());

		if (root["BaseColorMap"])
			material->BaseColorMap = root["BaseColorMap"].as<std::string>();
		if (root["NormalMap"])
			material->NormalMap = root["NormalMap"].as<std::string>();
		if (root["MetallicRoughnessMap"])
			material->MetallicRoughnessMap = root["MetallicRoughnessMap"].as<std::string>();
		if (root["EmissiveMap"])
			material->EmissiveMap = root["EmissiveMap"].as<std::string>();

		if (Utils::ReadVec2(root["UVTiling"], v2))
			material->UVTiling = v2;
		if (Utils::ReadVec2(root["UVOffset"], v2))
			material->UVOffset = v2;

		return material;
	}

} // namespace Candy
