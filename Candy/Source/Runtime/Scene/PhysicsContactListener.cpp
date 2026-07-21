#include "CandyPCH.h"
#include "Runtime/Scene/PhysicsContactListener.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Entity.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Scripting/ScriptSystem.h"

#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_contact.h"

namespace Candy {

PhysicsContactListener::PhysicsContactListener(Scene* scene)
	: m_Scene(scene)
{
}

void PhysicsContactListener::BeginContact(b2Contact* contact)
{
	uintptr_t userDataA = contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	uintptr_t userDataB = contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	if (userDataA == 0 || userDataB == 0)
		return;

	CollisionEvent event;
	event.EntityA = static_cast<entt::entity>(static_cast<uint32_t>(userDataA));
	event.EntityB = static_cast<entt::entity>(static_cast<uint32_t>(userDataB));
	event.IsEnter = true;
	m_EventQueue.push_back(event);

	// CANDY_CORE_TRACE("BeginContact: entity {0} <-> {1}", (uint32_t)event.EntityA, (uint32_t)event.EntityB);
}

void PhysicsContactListener::EndContact(b2Contact* contact)
{
	uintptr_t userDataA = contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	uintptr_t userDataB = contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	if (userDataA == 0 || userDataB == 0)
		return;

	CollisionEvent event;
	event.EntityA = static_cast<entt::entity>(static_cast<uint32_t>(userDataA));
	event.EntityB = static_cast<entt::entity>(static_cast<uint32_t>(userDataB));
	event.IsEnter = false;
	m_EventQueue.push_back(event);

	// CANDY_CORE_TRACE("EndContact: entity {0} <-> {1}", (uint32_t)event.EntityA, (uint32_t)event.EntityB);
}

void PhysicsContactListener::Flush()
{
	auto& registry = m_Scene->GetRegistry();

	for (const auto& event : m_EventQueue)
	{
		// Check that both entities are still valid (might have been destroyed during Step)
		if (!registry.valid(event.EntityA) || !registry.valid(event.EntityB))
			continue;

		Entity entityA = { event.EntityA, m_Scene };
		Entity entityB = { event.EntityB, m_Scene };

		auto& scriptSystem = ScriptSystem::Get();

		if (event.IsEnter)
		{
			scriptSystem.DispatchCollisionEnter(entityA.GetUUID(), entityB.GetUUID());
			scriptSystem.DispatchCollisionEnter(entityB.GetUUID(), entityA.GetUUID());
		}
		else
		{
			scriptSystem.DispatchCollisionExit(entityA.GetUUID(), entityB.GetUUID());
			scriptSystem.DispatchCollisionExit(entityB.GetUUID(), entityA.GetUUID());
		}
	}

	m_EventQueue.clear();
}

} // namespace Candy
