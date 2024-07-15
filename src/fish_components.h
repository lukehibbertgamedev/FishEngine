
#pragma once

#include <vk_types.h>
#include <glm/gtx/quaternion.hpp>

namespace Fish {

	namespace Component {

		struct Transform {
			glm::vec3 position = glm::vec3(0.0f);
			//glm::quat rotation;
			glm::vec3 scale = glm::vec3(1.0f);
			glm::vec3 eulerRotation = glm::vec3(0.0f);
		};
	}
}