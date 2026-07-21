#pragma once

#include "box2d/b2_world_callbacks.h"

#include "entt.hpp"
#include <vector>

namespace Candy {

class Scene;

struct CollisionEvent
{
	entt::entity EntityA;
	entt::entity EntityB;
	bool IsEnter; // true = BeginContact, false = EndContact
};

class PhysicsContactListener : public b2ContactListener
{
public:
	explicit PhysicsContactListener(Scene* scene);

	void BeginContact(b2Contact* contact) override;
	void EndContact(b2Contact* contact) override;

	void Flush();

private:
	Scene* m_Scene;
	std::vector<CollisionEvent> m_EventQueue;
};

} // namespace Candy
