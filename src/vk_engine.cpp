//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

// We set a *global* pointer for the vulkan engine singleton reference. 
// We do that instead of a typical singleton because we want to control explicitly when is the class initalized and destroyed. 
// The normal Cpp singleton pattern doesnt give us control over that.
VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    m_pWindow = SDL_CreateWindow(
        "Fish Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        m_WindowExtents.width,
        m_WindowExtents.height,
        window_flags);

    // everything went fine
    m_IsInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (m_IsInitialized) {

        SDL_DestroyWindow(m_pWindow);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // nothing yet
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {

        // Handle events on queue.
        // This will ask SDL for all of the events that the OS has sent to the application
        // during the last frame. Here, we can check for things like keyboard events, mouse input, etc...
        while (SDL_PollEvent(&e) != 0) {

            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    m_StopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    m_StopRendering = false;
                }
            }

            // Quick example of getting a keypress event and logging it.
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_SPACE:
                    fmt::println("Space");
                    break;
                }

            }
        }

        // do not draw if condition is met
        if (m_StopRendering) {

            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}