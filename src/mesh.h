
#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/vec3.hpp>

namespace Fish {

	struct VertexInputDescription {
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
	};

	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 colour;

		static VertexInputDescription get_vertex_description();
	};

	struct Mesh {
		std::vector<Vertex> vertices;

		AllocatedBuffer vertexBuffer;
	};

}