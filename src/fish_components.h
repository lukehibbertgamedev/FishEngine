
#pragma once

#include <vk_types.h>
#include <glm/gtx/quaternion.hpp>

namespace Fish {

	namespace Component {

		struct Transform {
			glm::vec3 position;
			//glm::quat rotation;
			glm::vec3 scale;
			glm::vec3 eulerRotation;
		};
	}
}