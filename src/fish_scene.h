
#pragma once

#include <fish_camera.h>
#include <fish_loader.h>
#include <fish_gpu_data.h>
#include <fish_resource_manager.h>

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
		//std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> loadedScenes;
		std::unordered_map<std::string, Fish::ResourceData::Object> objectsInScene;
		std::string sceneName = "_";

		// Containers that will be populated on load() to be accessed to set data.
		Fish::ResourceData::Camera outCameraCache;
		std::vector<Fish::ResourceData::Object> outObjectCache;

		// Let the json handler save our scene.
		void save();

		// Let the json handler obtain all written data and store it in temporary structures.
		void load();

		void clear_scene();
	};

	class SceneManager {
	public:

		std::unordered_map<std::string, Scene> scenes;
		Scene* pActiveScene = nullptr;

		void create_new_scene(const std::string& name);
		void switch_scene(const std::string& sceneNameToLoad); // Todo: Overload this function to go by sceneIndex
		// void save_active_scene();
		// void load_scene(); // If selected a file, it will load that scene from file and set to active.
	};
}
