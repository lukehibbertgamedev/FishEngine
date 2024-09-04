
#pragma once

#include <fish_camera.h>
#include <fish_loader.h>
#include <fish_gpu_data.h>

#include <string>
#include <memory>
#include <unordered_map>

#include <fish_json_handler.h>

namespace Fish {

	class Scene {
	public:

		// Scene Data.
		Fish::Camera camera;												// A handle to our camera so we can move around our scene (expanded to current/main camera in future).
		Fish::GPU::GPUSceneData sceneData;
		VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
		std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> loadedScenes;

		// Let the json handler save our scene.
		void save();
	};
}
