#include "fish_camera.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Fish::Camera::get_view_matrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), m_Position);
    glm::mat4 cameraRotation = get_rotation_matrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Fish::Camera::get_rotation_matrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix
    glm::quat pitchRotation = glm::angleAxis(m_Pitch, glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(m_Yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Fish::Camera::processSDLEvent(SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { m_Velocity.z = -1; }  // Forward
        if (e.key.keysym.sym == SDLK_s) { m_Velocity.z = 1; }   // Backward
        if (e.key.keysym.sym == SDLK_a) { m_Velocity.x = -1; }  // Left
        if (e.key.keysym.sym == SDLK_d) { m_Velocity.x = 1; }   // Right
        if (e.key.keysym.sym == SDLK_q) { m_Velocity.y = 1; }   // Down
        if (e.key.keysym.sym == SDLK_e) { m_Velocity.y = -1; }  // Up
    }                                     
                                          
    if (e.type == SDL_KEYUP) {            
        if (e.key.keysym.sym == SDLK_w) { m_Velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { m_Velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { m_Velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { m_Velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_q) { m_Velocity.y = 0; }
        if (e.key.keysym.sym == SDLK_e) { m_Velocity.y = 0; }
    }

    if (e.type == SDL_MOUSEMOTION) {
        m_Yaw += e.motion.xrel / 200.f;
        m_Pitch -= e.motion.yrel / 200.f;
    }
}

void Fish::Camera::update()
{
	glm::mat4 cameraRotation = get_rotation_matrix();
	m_Position += glm::vec3(cameraRotation * glm::vec4(m_Velocity * 0.5f, 0.f));
}
