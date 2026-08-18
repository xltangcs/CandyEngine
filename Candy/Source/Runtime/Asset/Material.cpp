#include "CandyPCH.h"

#include "Runtime/Asset/Material.h"
#include "Runtime/Asset/ShaderCache.h"
#include "Runtime/Core/FileSystem.h"

#include <yaml-cpp/yaml.h>

namespace Candy {

	namespace Utils {

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

		static void EmitVec4(YAML::Emitter& out, const glm::vec4& v)
		{
			out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		}

		static void EmitParamValue(YAML::Emitter& out, const ShaderParamValue& value)
		{
			switch (value.index())
			{
				case 0:  out << std::get<float>(value);        break;
				case 1:  out << std::get<int>(value);         break;
				case 2:  out << std::get<bool>(value);        break;
				case 3:  EmitVec2(out, std::get<glm::vec2>(value)); break;
				case 4:  EmitVec3(out, std::get<glm::vec3>(value)); break;
				case 5:  EmitVec4(out, std::get<glm::vec4>(value)); break;
				case 6:  out << std::get<std::string>(value); break;
				default: out << 0.0f;                         break;
			}
		}

		// Heuristic YAML -> ShaderParamValue read. Precise typing is re-applied
		// in GetShaderParameters via coercion against the reflected parameter.
		static bool ReadParamValue(const YAML::Node& node, ShaderParamValue& out)
		{
			if (node.IsSequence())
			{
				if (node.size() == 2) { out = glm::vec2(node[0].as<float>(), node[1].as<float>()); return true; }
				if (node.size() == 3) { out = glm::vec3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>()); return true; }
				if (node.size() == 4) { out = glm::vec4(node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()); return true; }
				return false;
			}
			if (!node.IsScalar())
				return false;

			const std::string text = node.as<std::string>();
			if (text.rfind("VFS://", 0) == 0)
			{
				out = text;
				return true;
			}
			if (text == "true")
			{
				out = true;
				return true;
			}
			if (text == "false")
			{
				out = false;
				return true;
			}
			try
			{
				size_t pos = 0;
				const int i = std::stoi(text, &pos);
				if (pos == text.size())
				{
					out = i;
					return true;
				}
			}
			catch (...) {}
			try
			{
				size_t pos = 0;
				const float f = std::stof(text, &pos);
				if (pos == text.size())
				{
					out = f;
					return true;
				}
			}
			catch (...) {}
			out = text;
			return true;
		}

		// Best-effort conversion of an override value to the reflected type, so
		// hand-edited .mat files (e.g. "1" instead of "1.0") still apply.
		static ShaderParamValue CoerceParamValue(const ShaderParamValue& src, const ShaderParamValue& target)
		{
			const size_t targetIndex = target.index();
			if (src.index() == targetIndex)
				return src;

			std::optional<float> scalar;
			switch (src.index())
			{
				case 0:  scalar = std::get<float>(src);      break;
				case 1:  scalar = static_cast<float>(std::get<int>(src)); break;
				case 2:  scalar = std::get<bool>(src) ? 1.0f : 0.0f; break;
				default: return src;
			}
			switch (targetIndex)
			{
				case 0:  return *scalar;
				case 1:  return static_cast<int>(*scalar);
				case 2:  return *scalar != 0.0f;
				default: return src;
			}
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

			out << YAML::Key << "ShaderPath" << YAML::Value << ShaderPath;

			if (!ShaderParams.empty())
			{
				out << YAML::Key << "Params" << YAML::Value;
				out << YAML::BeginMap;
				for (const auto& [name, value] : ShaderParams)
				{
					out << YAML::Key << name << YAML::Value;
					Utils::EmitParamValue(out, value);
				}
				out << YAML::EndMap;
			}

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

		if (root["Name"])
			material->Name = root["Name"].as<std::string>();

		if (root["ShaderPath"])
			material->ShaderPath = root["ShaderPath"].as<std::string>();

		if (root["Params"])
		{
			for (const auto& paramNode : root["Params"])
			{
				const std::string name = paramNode.first.as<std::string>();
				ShaderParamValue value;
				if (Utils::ReadParamValue(paramNode.second, value))
					material->ShaderParams[name] = value;
			}
		}

		return material;
	}

	bool Material::GetShaderParameters(std::vector<ShaderParameter>& outParameters) const
	{
		if (ShaderPath.empty())
			return false;

		const std::vector<ShaderParameter>* reflected = ShaderCache::Get().GetParameters(ShaderPath);
		if (!reflected || reflected->empty())
			return false;

		outParameters = *reflected;
		for (auto& param : outParameters)
		{
			auto it = ShaderParams.find(param.Name);
			if (it == ShaderParams.end())
				continue;
			param.Default = Utils::CoerceParamValue(it->second, param.Default);
		}
		return true;
	}

} // namespace Candy
