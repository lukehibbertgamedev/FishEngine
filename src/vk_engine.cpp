//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_images.h>
#include <vk_initializers.h>

#include <VkBootstrap.h> // bootstrap library (if you want to set up without this, see https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code)

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

    init_vulkan();

    init_swapchain();

    init_commands();

    init_synchronisation_structures();

    // everything went fine
    m_IsInitialized = true;
}

void VulkanEngine::init_vulkan()
{

    // Abstract the creation of a vulkan context.
    vkb::InstanceBuilder builder;

    //make the vulkan instance, with basic debug features
    vkb::Result<vkb::Instance> instance = 
        builder.set_app_name("Example Vulkan Application")
        .set_engine_name("Fish")
        .request_validation_layers(kUseValidationLayers)
        .use_default_debug_messenger()
        //.require_api_version(1, 3, 0)
        .desire_api_version(VKB_VK_API_VERSION_1_3)
        .build();

    if (!instance) {
        fmt::println("Failed to create Vulkan instance.");
    }

    vkb::Instance vkb_instance = instance.value();

    //grab the instance 
    m_VkInstance = vkb_instance.instance;
    m_DebugMessenger = vkb_instance.debug_messenger;

    SDL_Vulkan_CreateSurface(m_pWindow, m_VkInstance, &m_SurfaceKHR);

    //vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    //vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    //use vkbootstrap to select a gpu. 
    //We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_instance };
    vkb::PhysicalDevice physicalDevice = selector
        //.set_minimum_version(1, 3)
        .set_desired_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(m_SurfaceKHR)
        .select()
        .value();


    //create the final vulkan device
    vkb::DeviceBuilder deviceBuilder { physicalDevice };

    vkb::Device vkb_device = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    m_Device = vkb_device.device;
    m_PhysicalDevice = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    m_GraphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    m_GraphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

}

void VulkanEngine::init_swapchain()
{
    create_swapchain(m_WindowExtents.width, m_WindowExtents.height);
}

void VulkanEngine::init_commands()
{
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < kFrameOverlap; i++) {

        VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_Frames[i].commandBuffer));
    }
}

void VulkanEngine::init_synchronisation_structures()
{
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < kFrameOverlap; i++) {
        VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_Frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].renderSemaphore));
    }
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{ m_PhysicalDevice, m_Device, m_SurfaceKHR};

    m_SwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = m_SwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    m_SwapchainExtent = vkbSwapchain.extent;

    //store swapchain and its related images
    m_Swapchain = vkbSwapchain.swapchain;
    m_SwapchainImages = vkbSwapchain.get_images().value();
    m_SwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < m_SwapchainImageViews.size(); i++) {

        vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::cleanup()
{
    // Delete in the opposite order to what they were created, to avoid dependency errors.

    if (m_IsInitialized) {

        //make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(m_Device);

        for (int i = 0; i < kFrameOverlap; i++) {
            vkDestroyCommandPool(m_Device, m_Frames[i].commandPool, nullptr);

            //destroy sync objects
            vkDestroyFence(m_Device, m_Frames[i].renderFence, nullptr);
            vkDestroySemaphore(m_Device, m_Frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(m_Device, m_Frames[i].swapchainSemaphore, nullptr);
        }

        destroy_swapchain();

        vkDestroySurfaceKHR(m_VkInstance, m_SurfaceKHR, nullptr);

        vkDestroyDevice(m_Device, nullptr);

        vkb::destroy_debug_utils_messenger(m_VkInstance, m_DebugMessenger);

        vkDestroyInstance(m_VkInstance, nullptr);

        SDL_DestroyWindow(m_pWindow);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    const unsigned int timeout = 1000000000;

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(m_Device, 1, &GetCurrentFrame().renderFence, true, timeout));
    VK_CHECK(vkResetFences(m_Device, 1, &GetCurrentFrame().renderFence));

    //request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, timeout, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));

    // get local frame command buffer
    VkCommandBuffer cmd = GetCurrentFrame().commandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    //start the command buffer recording
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make the swapchain image into writeable mode before rendering
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearColorValue clearValue;
    float framePeriod = 120.f;
    float flash = std::abs(std::sin(m_FrameNumber / framePeriod));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    //clear image
    vkCmdClearColorImage(cmd,m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    //make the swapchain image into presentable mode
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue. 
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    //VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
    //VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, GetCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

    VkSubmitInfo submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

    //increase the number of frames drawn
    m_FrameNumber++;
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