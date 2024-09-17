#pragma once

// name: fish_resource_data.h
// desc: A middleman between our JSON handler and our engine's handle to all objects.
// auth: Luke Hibbert

#include <string>
#include <glm/vec3.hpp>
//#include <fish_loader.h>
#include <fish_components.h>

namespace Fish {
	namespace ResourceData {
		
		struct Camera {
			glm::vec3 position;
			float pitch;
			float yaw;
		};

		struct Model {
			//std::shared_ptr<Fish::Loader::LoadedGLTF> current;
		};

		struct RenderableObject {
			//std::shared_ptr<Fish::Loader::LoadedGLTF> model;
			Fish::Component::Mesh mesh;
			Fish::Component::Transform transform;
			std::string name;
		};

		// Todo: Replace instances of this with RenderableObject.
		struct Object {
			Fish::Component::Transform transform;
			std::string name;

			Model model; // Model data for this object in the scene, this can be changed to any loadedGltf meshes container.
		};
	}
}
