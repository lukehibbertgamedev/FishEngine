#include "fish_ecs_systems.h"

void Fish::ECS::System::Physics::init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator)
{
	m_Coordinator = coordinator;
}

void Fish::ECS::System::Physics::update(float deltatime)
{
	for (Fish::ECS::Entity const& entity : mEntities)
	{
		Fish::Component::RigidBody& rigidbody = m_Coordinator->GetComponent<Fish::Component::RigidBody>(entity);
		Fish::Component::Transform& transform = m_Coordinator->GetComponent<Fish::Component::Transform>(entity);
		
		transform.eulerRotation += rigidbody.velocity * deltatime;
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
