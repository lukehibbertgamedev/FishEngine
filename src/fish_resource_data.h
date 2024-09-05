#pragma once

// name: fish_resource_data.h
// desc: A middleman between our JSON handler and our engine's handle to all objects.
// auth: Luke Hibbert

#include <string>
#include <glm/vec3.hpp>

namespace Fish {
	namespace ResourceData {
		
		struct Camera {
			glm::vec3 position;
			float pitch;
			float yaw;
		};

		struct Object {
			glm::vec3 position;
			glm::vec3 rotation;
			glm::vec3 scale;
			std::string name;
		};
	}
}
