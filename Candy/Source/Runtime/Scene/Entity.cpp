#include "CandyPCH.h"
#include "Runtime/Scene/Entity.h"

namespace Candy {
	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene)
	{
	}
}