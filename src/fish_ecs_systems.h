#pragma once
#include <set>
#include <fish_ecs.h>
#include <fish_components.h>

namespace Fish {

	namespace ECS {

		namespace System {

			// Rudimentary physics system for testing the ECS implementation.
			class Physics : public Fish::ECS::SystemBase {
			public:
				void init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator);
				void update(float deltatime);
			private:
				std::shared_ptr<Fish::ECS::Coordinator> m_Coordinator;
			};

			// Rendering system used to render any entity.
			class Render : public Fish::ECS::SystemBase {
			public:
				void init(std::shared_ptr<Fish::ECS::Coordinator>& coordinator);
				void update(float deltatime);
				void update_model_matrix(const Fish::Component::Transform& transform, Fish::Component::Mesh& mesh);
			private:
				std::shared_ptr<Fish::ECS::Coordinator> m_Coordinator;
			};
		}
	}
}