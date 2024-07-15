
#pragma once

#include <vk_types.h>
#include <glm/gtx/quaternion.hpp>

#define FISH_TRANSFORM_POSITION 1
#define FISH_TRANSFORM_EULER 2
#define FISH_TRANSFORM_SCALE 3

namespace Fish {

	namespace Component {

		struct Transform {
			glm::vec3 position = glm::vec3(0.0f);
			//glm::quat rotation;
			glm::vec3 scale = glm::vec3(1.0f);
			glm::vec3 eulerRotation = glm::vec3(0.0f);

			void set(uint8_t macro, const glm::vec3& value)
			{
				switch (macro) 
				{
				case 1:
					position = value;
					break;
				case 2:
					eulerRotation = value;
					break;
				case 3:
					scale = value;
					break;
				default:
					assert(false && "No matching macro");
					break;
				}
			}
		};
	}
}