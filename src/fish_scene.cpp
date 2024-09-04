#include "fish_scene.h"

void Fish::Scene::save()
{
	Fish::JSON::Handler handler("../../src/test_save.json", loadedScenes);
	//handler.test_save();
	handler.save();
}
