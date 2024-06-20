
#pragma once

#include <fish_scene.h>
#include <fish_resource.h>
#include <vector>

namespace Fish {

	class Scene {
	public:
		std::vector<Fish::Resource::RenderObject> GetSceneObjects() { return m_SceneObjects; }

	private:
		std::vector<Fish::Resource::RenderObject> m_SceneObjects;

	};
}
