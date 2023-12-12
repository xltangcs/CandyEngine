#pragma once

#include "Candy/Scene/Entity.h"

namespace Candy {

	class ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() {}
		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}
	protected:
		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate(Timestep ts) {}
	private:
		Entity m_Entity;
		friend class Scene;
	};

}