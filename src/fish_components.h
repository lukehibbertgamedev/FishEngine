
#pragma once

#include <vk_types.h>
#include <fish_loader.h>

namespace Fish {

	namespace Component {

		//struct Transform {
		//	glm::vec3 position;
		//	glm::vec3 eulerRotation;
		//	glm::vec3 scale/* = glm::vec3(1.0f)*/;
		//};

		struct Mesh {
			std::shared_ptr<Fish::Loader::LoadedGLTF> model;
		};

		struct RigidBody {
			glm::vec3 velocity;
			glm::vec3 acceleration;
		};

		struct Gravity {
			glm::vec3 force/* = glm::vec3(0.0f, -9.81f, 0.0f)*/;
		};
	}
}