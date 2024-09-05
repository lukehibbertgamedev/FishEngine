#include "fish_scene.h"

void Fish::Scene::save()
{
	Fish::JSON::Handler handler("../../src/test_save.json");
	handler.save(loadedScenes, camera);
}

void Fish::Scene::load()
{
	Fish::JSON::Handler handler("../../src/test_save.json");
	handler.load_camera_data(cameraCache);
	handler.load_object_data(objectCache);
}
