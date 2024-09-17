#include "fish_ecs_systems.h"

void Fish::ECS::System::Physics::init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator)
{
	m_Coordinator = coordinator;
}

void Fish::ECS::System::Physics::update(float deltatime)
{
	for (Fish::ECS::Entity const& entity : mEntities)
	{
		auto& transform = m_Coordinator->GetComponent<Fish::Component::Transform>(entity);
		auto& rigidBody = m_Coordinator->GetComponent<Fish::Component::RigidBody>(entity);
		auto const& gravity = m_Coordinator->GetComponent<Fish::Component::Gravity>(entity);

		//transform.position += rigidBody.velocity * deltatime;
		//rigidBody.velocity += gravity.force * deltatime;
		transform.rotation.y += 75.0f * deltatime;
	}
}

// ---------------------------------------------------------------------------------------------------------------- // 

void Fish::ECS::System::Render::init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator)
{
	m_Coordinator = coordinator;
}

void Fish::ECS::System::Render::update(float deltatime)
{
	for (Fish::ECS::Entity const& entity : mEntities)
	{		
		Fish::Component::Transform& transform = m_Coordinator->GetComponent<Fish::Component::Transform>(entity);
		Fish::Component::Mesh& mesh = m_Coordinator->GetComponent<Fish::Component::Mesh>(entity);
		bool valid = (mesh.model != nullptr);
		
		if (valid) {
			update_model_matrix(transform, mesh);
		}

		// Todo: check mesh dirty flag - only update if dirty		
		// if not mesh.modelMatAlreadyUpdated
		//	update_model_matrix
		//	mesh.modelMatAlreadyUpdated = true

		// Todo: concatenate child model matrix with parent - for relative update
		// node has refresh transform for all children takes in only parent matrix

		// mesh.modelMatAlreadyUpdated = false // to be set true elsewhere if necessary
	}
}

void Fish::ECS::System::Render::update_model_matrix(const Fish::Component::Transform& transform, Fish::Component::Mesh& mesh)
{
}
