
#pragma once

#include <fish_scene.h>
#include <fish_resource.h>
#include <vector>

namespace Fish {

	class Scene {
	public:
		//std::vector<Fish::Resource::RenderObject> GetSceneObjectsVal() { return m_SceneObjects; }
		//std::vector<Fish::Resource::RenderObject>& GetSceneObjectsRef() { return m_SceneObjects; }

		std::vector<Fish::Resource::RenderObject> m_SceneObjects {};
	private:

	};
}
