#include "fish_ecs_systems.h"

void Fish::ECS::System::Physics::init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator)
{
	m_Coordinator = coordinator;
}

void Fish::ECS::System::Physics::update(float deltatime)
{
	for (Fish::ECS::Entity const& entity : mEntities)
	{
		auto& rigidBody = m_Coordinator->GetComponent<Fish::Component::RigidBody>(entity);
		auto& transform = m_Coordinator->GetComponent<Fish::Component::Transform>(entity);
		auto const& gravity = m_Coordinator->GetComponent<Fish::Component::Gravity>(entity);

		transform.position += rigidBody.velocity * deltatime;

		rigidBody.velocity += gravity.force * deltatime;
	}
}

// ---------------------------------------------------------------------------------------------------------------- // 

void Fish::ECS::System::Renderer::init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator)
{
	m_Coordinator = coordinator;
}

void Fish::ECS::System::Renderer::update(float deltatime)
{
	for (Fish::ECS::Entity const& entity : mEntities)
	{
		// if mesh is active - don't update model matrix
		// check mesh dirty flag - only update if dirty
		// concatenate child model matrix with parent - for relative update
	}
}
