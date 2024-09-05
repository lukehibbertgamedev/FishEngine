#include "fish_scene.h"

void Fish::Scene::save()
{
	Fish::JSON::Handler handler("../../src/test_save.json");
	handler.save(loadedScenes, camera);
}

void Fish::Scene::load()
{
	// Read data from file.
	Fish::JSON::Handler handler("../../src/test_save.json");
	handler.load_object_data(objectCache, cameraCache);

	std::unordered_map<std::string, size_t> nameToIndexMap;
	for (size_t i = 0; i < objectCache.size(); ++i) {
		nameToIndexMap[objectCache[i].name] = i;
	}

	// Set runtime data.
	for (auto& [key, value] : loadedScenes) {

		auto it = nameToIndexMap.find(key);
		if (it != nameToIndexMap.end()) {
			Fish::Loader::LoadedGLTF& obj = *value;
			size_t index = it->second;

			Fish::ResourceData::Object& objData = objectCache[index];
			obj.transform.position = glm::vec4(objData.position, 1.0f);
			obj.transform.rotation = glm::vec4(objData.rotation, 1.0f);
			obj.transform.scale = glm::vec4(objData.scale, 1.0f);
		}
		else FISH_FATAL("No matching object found for: " + key);
	}
}
