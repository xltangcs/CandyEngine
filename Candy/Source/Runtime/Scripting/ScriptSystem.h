#pragma once
#include <unordered_map>
#include <memory>

#include "Runtime/Core/Base.h"
#include "Runtime/Core/UUID.h"
#include "Runtime/Core/Timestep.h"

namespace Candy
{
	class Entity;
	class ScriptObject;
	struct ScriptInstance;

	class ScriptSystem {
	public:
		static ScriptSystem& Get();

		void InitPython();
		void ShutdownPython();
		
		void InstantiateScript(Entity& entity);
		void DestroyScript(UUID entityID);

		void OnRuntimeStart();
		void OnUpdateRuntime(Timestep ts);
		void OnRuntimeStop();

		void CallFunction(Entity entity, const std::string& funcName);

		void DispatchCollisionEnter(UUID entityID, UUID otherID);
		void DispatchCollisionExit(UUID entityID, UUID otherID);

	private:
		ScriptSystem() = default;
		~ScriptSystem() = default;
		ScriptSystem(const ScriptSystem&) = delete;
		ScriptSystem& operator=(const ScriptSystem&) = delete;
		
		std::unordered_map<UUID, std::unique_ptr<ScriptInstance>> m_Instances;
		bool m_PythonInitialized = false;
	};
}
