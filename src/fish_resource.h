#pragma once

#include <vk_types.h>
#include <fish_components.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace Fish {

	namespace Resource {

		struct VertexInputDescription {
			std::vector<VkVertexInputBindingDescription> bindings;
			std::vector<VkVertexInputAttributeDescription> attributes;
		};

		struct Vertex {
			glm::vec3 position;
			glm::vec3 normal;
			glm::vec3 colour;
			glm::vec2 uv;

			static VertexInputDescription get_vertex_description();
		};

		struct Mesh {
			std::vector<Vertex> vertices;

			AllocatedBuffer vertexBuffer;

			bool load_from_obj(const char* filename);
		};

		struct Texture {
			AllocatedImage image;

			VkImageView imageView;
		};

		struct Material {
			VkDescriptorSet textureSet{ VK_NULL_HANDLE }; //texture defaulted to null
			VkPipeline pipeline;
			VkPipelineLayout pipelineLayout;
		};

		struct RenderObject {
			Fish::Component::Transform transform; 
			Mesh* pMesh;
			Material* pMaterial;
			glm::mat4 transformMatrix;
		};
	}
}