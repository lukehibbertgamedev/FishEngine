
#pragma once

#include <vk_types.h>

namespace Fish {

	namespace GPU {

		struct CameraData {
			glm::mat4 viewMatrix;
			glm::mat4 projectionMatrix;
			glm::mat4 viewProjectionMatrix;
		};

		struct ObjectData {
			glm::mat4 modelMatrix;
		};

		struct SceneData {
			glm::vec4 fogColour;			// W is for exponent.
			glm::vec4 fogDistances;			// X for min. Y for max. ZW unused.
			glm::vec4 ambientColour;
			glm::vec4 sunlightDirection;	// W is for intensity/power.
			glm::vec4 sunlightColour;
		};
	}
}