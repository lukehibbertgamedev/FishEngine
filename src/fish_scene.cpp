#include "fish_scene.h"

void Fish::Scene::save()
{
	Fish::JSON::Handler handler("../../src/default_scene_01.json");
	handler.serialise_scene_data(sceneName, loadedScenes, camera);
}

void Fish::Scene::load()
{
	// Read data from file.
	Fish::JSON::Handler handler("../../src/default_scene_01.json");
	handler.parse_scene_data(sceneName, outObjectCache, outCameraCache); // Populate data containers.

	// Ensure the objectCache index matches the object that we want to apply that data to.
	std::unordered_map<std::string, size_t> nameToIndexMap;
	for (size_t i = 0; i < outObjectCache.size(); ++i) {
		nameToIndexMap[outObjectCache[i].name] = i;
	}

	// Set object runtime data.
	for (auto& [key, value] : loadedScenes) {
		auto it = nameToIndexMap.find(key);
		if (it != nameToIndexMap.end()) {
			Fish::Loader::LoadedGLTF& obj = *value;
			size_t index = it->second;

			Fish::ResourceData::Object& objData = outObjectCache[index];
			obj.transform.position = glm::vec4(objData.position, 1.0f);
			obj.transform.rotation = glm::vec4(objData.rotation, 1.0f);
			obj.transform.scale = glm::vec4(objData.scale, 1.0f);
		}
		else FISH_FATAL("No matching object found for: " + key);
	}

	// Set camera runtime data.
	camera.m_Position = outCameraCache.position;
	camera.m_Pitch = outCameraCache.pitch;
	camera.m_Yaw = outCameraCache.yaw;
}
