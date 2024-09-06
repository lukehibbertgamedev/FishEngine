#pragma once

// name: fish_camera.h
// desc: A class for our interactive camera so we can move around a scene.
// desc: Most of the code is modified from the Vulkan Guide by vblanco20-1 on Git.
// auth: Luke Hibbert

#include <vk_types.h>
#include <SDL_events.h>

namespace Fish {

	class Camera {
	public:

		glm::vec3 m_Velocity{ 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };

		float m_Pitch{ 0.0f };
		float m_Yaw{ 0.0f };

		bool m_Toggle = false;

		glm::mat4 get_view_matrix();
		glm::mat4 get_rotation_matrix();

		void processSDLEvent(SDL_Event& e);

		void update();
	};
}