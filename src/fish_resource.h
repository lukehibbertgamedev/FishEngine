#pragma once

// name: fish_resource.h
// desc: A collection of structures related to resources that we may load.
// auth: Luke Hibbert

#include <vk_types.h>
#include <fish_loader.h>

namespace Fish {

	namespace Resource {

		// 1.3 - Abstraction of everything required to render an object.
		struct RenderObject {
			uint32_t indexCount;					// Number of indices.
			uint32_t firstIndex;					// First index in the index buffer.
			VkBuffer indexBuffer;					// A container/buffer of all indices.

			MaterialInstance* material;				// A material to be applied to this render object.
			Fish::Loader::Bounds bounds;			// ...
			glm::mat4 transform;					// Transformation matrix.
			VkDeviceAddress vertexBufferAddress;	// ...
		};
	}
}