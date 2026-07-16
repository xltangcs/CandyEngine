#pragma once

#include "Candy/Core/Base.h"
#include "Scene.h"

#include <yaml-cpp/yaml.h>

namespace Candy {

	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void Serialize(const std::string& filepath);
		void SerializeRuntime(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeFromString(const std::string& yamlContent);
		bool DeserializeRuntime(const std::string& filepath);
	private:
		bool DeserializeNode(YAML::Node& data);
		Ref<Scene> m_Scene;
	};

}