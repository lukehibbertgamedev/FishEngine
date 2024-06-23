
#pragma once

#include <vk_types.h>
#include <SDL_events.h>

namespace Fish {

	class Camera {
	public:

		glm::vec3 m_Velocity{ 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Position{ -20.0f, 11.0f, 50.0f };

		float m_Pitch{ 0.0f };
		float m_Yaw{ 0.0f };

		bool m_Toggle = true;

		glm::mat4 get_view_matrix();
		glm::mat4 get_rotation_matrix();

		void processSDLEvent(SDL_Event& e);

		void update();
	};

}