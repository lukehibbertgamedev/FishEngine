#include "fish_scene.h"

void Fish::Scene::save()
{
	FISH_LOG("Saving scene data...");

	Fish::JSON::Handler handler("../../src/default_scene_01.json");
	handler.serialise_scene_data(sceneName, objectsInScene, camera);
}

void Fish::Scene::load()
{
	FISH_LOG("Loading scene data...");

	// Read data from file.
	Fish::JSON::Handler handler("../../src/default_scene_01.json");
	handler.parse_scene_data(sceneName, objectsInScene, outCameraCache); // Populate data containers.

	// Ensure the objectCache index matches the object that we want to apply that data to.
	std::unordered_map<std::string, size_t> nameToIndexMap;
	size_t index = 0;
	for (const auto& [name, object] : objectsInScene) {
		nameToIndexMap[name] = index;
		++index;
	}

	// Set object runtime data.
	for (auto& [key, value] : objectsInScene) {
		auto it = nameToIndexMap.find(key);
		if (it != nameToIndexMap.end()) {
			//Fish::ResourceData::Object& obj = value;
			//size_t index = it->second;
			Fish::ResourceData::Object& objData = objectsInScene[key];
			Fish::Loader::LoadedGLTF& loadedGltf = *ResourceManager::Get().loadedResources[key];
			loadedGltf.transform.position = glm::vec4(objData.position, 1.0f); // .transform.
			loadedGltf.transform.rotation = glm::vec4(objData.rotation, 1.0f);
			loadedGltf.transform.scale = glm::vec4(objData.scale, 1.0f);
		}
		else FISH_FATAL("No matching object found for: " + key);
	}

	// Set camera runtime data.
	camera.m_Position = outCameraCache.position;
	camera.m_Pitch = outCameraCache.pitch;
	camera.m_Yaw = outCameraCache.yaw;
}

void Fish::Scene::clear_scene()
{
	// Clean up used memory.
}

// -- // 

void Fish::SceneManager::create_new_scene(const std::string& name)
{
	FISH_LOG("Creating new scene: " + name + "...");

	Scene previousScene = *pActiveScene;
	Scene newScene;
	scenes[name] = newScene;
	pActiveScene = &scenes[name];

	// Update runtime data
	previousScene.objectsInScene.clear();
}
