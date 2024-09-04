#include "fish_scene.h"

void Fish::Scene::save()
{
	Fish::JSON::Handler handler("../../src/test_save.json");
	//handler.test_save();
	handler.save(loadedScenes, camera);
}
