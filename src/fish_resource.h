#pragma once

#include <vk_types.h>
#include <fish_components.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>

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

			std::vector<uint32_t> indices;
			AllocatedBuffer indexBuffer;

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
			Fish::Component::Transform transform = {};
			Mesh* pMesh = nullptr;
			Material* pMaterial = nullptr;
			glm::mat4 transformMatrix = glm::mat4(1.0f);

			void update_model_matrix()
			{
				// Translation
				glm::mat4 translation = glm::translate(transform.position);

				// Rotation
				glm::mat4 x = glm::rotate(glm::radians(transform.eulerRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
				glm::mat4 y = glm::rotate(glm::radians(transform.eulerRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
				glm::mat4 z = glm::rotate(glm::radians(transform.eulerRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
				glm::mat4 rotation = z * y * x;

				// Scale
				glm::mat4 scale = glm::scale(transform.scale);

				// Model
				transformMatrix = scale * rotation * translation;
			}
		};
	}
}