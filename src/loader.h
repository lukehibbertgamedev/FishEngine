
#pragma once

#include <vk_types.h>
#include <vk_engine.h>

namespace Fish {

	namespace Loader {

		bool load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage);

	}
}