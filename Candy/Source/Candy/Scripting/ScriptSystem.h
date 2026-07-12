#pragma once
#include <unordered_map>
#include <memory>

#include "Candy/Core/Base.h"
#include "Candy/Core/UUID.h"
#include "Candy/Core/Timestep.h"

namespace Candy
{
	class Entity;
	class ScriptObject;
	struct ScriptInstance;

	class ScriptSystem {
	public:
		ScriptSystem();
		~ScriptSystem();

		void InitPython();
		void ShutdownPython();

		void InstantiateScript(Entity& entity);
		void DestroyScript(UUID entityID);

		void OnRuntimeStart();
		void OnUpdateRuntime(Timestep ts);
		void OnRuntimeStop();

		void CallFunction(Entity entity, const std::string& funcName);

	private:
		std::unordered_map<UUID, std::unique_ptr<ScriptInstance>> m_Instances;
	};
}