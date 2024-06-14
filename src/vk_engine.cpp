//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_images.h>
#include <vk_initializers.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <VkBootstrap.h> // bootstrap library (if you want to set up without this, see https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code)

#include <chrono>
#include <thread>
#include <fstream>

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

    init_main_renderpass();

    init_framebuffers();

    init_synchronisation_structures();

    init_pipelines();

    load_meshes();

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

    //initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_PhysicalDevice;
    allocatorInfo.device = m_Device;
    allocatorInfo.instance = m_VkInstance;
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);
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

    VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &commandPool));

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(commandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &commandBuffer));

    deletionQueue.push_function([=]() { vkDestroyCommandPool(m_Device, commandPool, nullptr); });
}

void VulkanEngine::init_main_renderpass()
{
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = m_SwapchainImageFormat;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VK_CHECK(vkCreateRenderPass(m_Device, &render_pass_info, nullptr, &m_MainRenderPass));

    deletionQueue.push_function([=]() { vkDestroyRenderPass(m_Device, m_MainRenderPass, nullptr); });
}

void VulkanEngine::init_framebuffers()
{
    //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = nullptr;

    fb_info.renderPass = m_MainRenderPass;
    fb_info.attachmentCount = 1;
    fb_info.width = m_WindowExtents.width;
    fb_info.height = m_WindowExtents.height;
    fb_info.layers = 1;

    //grab how many images we have in the swapchain
    const uint32_t swapchain_imagecount = m_SwapchainImages.size();
    m_FrameBuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

    //create framebuffers for each of the swapchain image views
    for (int i = 0; i < swapchain_imagecount; i++) {

        fb_info.pAttachments = &m_SwapchainImageViews[i];
        VK_CHECK(vkCreateFramebuffer(m_Device, &fb_info, nullptr, &m_FrameBuffers[i]));

        deletionQueue.push_function([=]() {
            vkDestroyFramebuffer(m_Device, m_FrameBuffers[i], nullptr);
            vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
        });
    }
}

void VulkanEngine::init_pipelines()
{
    // TODO: Load shaders in one function, and shorten the function into a "check_load" type function.

    VkShaderModule triangleFragShader;
    if (!load_shader_module("../../shaders/triangle.frag.spv", &triangleFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module");
    }
    else {
        fmt::println("Triangle fragment shader successfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!load_shader_module("../../shaders/triangle.vert.spv", &triangleVertexShader))
    {
        fmt::println("Error when building the triangle vertex shader module");
    }
    else {
        fmt::println("Triangle vertex shader successfully loaded");
    }

    VkShaderModule redTriangleFragShader;
    if (!load_shader_module("../../shaders/redTriangle.frag.spv", &redTriangleFragShader))
    {
        fmt::println("Error when building the red triangle fragment shader module");
    }
    else {
        fmt::println("Red triangle fragment shader successfully loaded");
    }

    VkShaderModule redTriangleVertexShader;
    if (!load_shader_module("../../shaders/redTriangle.vert.spv", &redTriangleVertexShader))
    {
        fmt::println("Error when building the red triangle vertex shader module");
    }
    else {
        fmt::println("Red triangle vertex shader successfully loaded");
    }

    VkShaderModule triangleMeshShader;
    if (!load_shader_module("../../shaders/triangleMesh.vert.spv", &triangleMeshShader))
    {
        fmt::println("Error when building the triangle mesh vertex shader module");
    }
    else {
        fmt::println("Triangle mesh vertex shader successfully loaded");
    }

    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(m_Device, &pipeline_layout_info, nullptr, &m_TrianglePipelineLayout));

    //build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;

    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));

    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));


    //vertex input controls how to read vertices from vertex buffers. We aren't using it yet
    pipelineBuilder.vertexInputState = vkinit::vertex_input_state_create_info();

    //input assembly is the configuration for drawing triangle lists, strips, or individual points.
    //we are just going to draw triangle list
    pipelineBuilder.inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    //build viewport and scissor from the swapchain extents
    pipelineBuilder.viewport.x = 0.0f;
    pipelineBuilder.viewport.y = 0.0f;
    pipelineBuilder.viewport.width = (float)m_WindowExtents.width;
    pipelineBuilder.viewport.height = (float)m_WindowExtents.height;
    pipelineBuilder.viewport.minDepth = 0.0f;
    pipelineBuilder.viewport.maxDepth = 1.0f;

    pipelineBuilder.scissor.offset = { 0, 0 };
    pipelineBuilder.scissor.extent = m_WindowExtents;

    //configure the rasterizer to draw filled triangles
    pipelineBuilder.rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    //we don't use multisampling, so just run the default one
    pipelineBuilder.multisampler = vkinit::multisampling_state_create_info();

    //a single blend attachment with no blending and writing to RGBA
    pipelineBuilder.colourBlendAttachment= vkinit::color_blend_attachment_state();

    //use the triangle layout we created
    pipelineBuilder.pipelineLayout = m_TrianglePipelineLayout;

    //finally build the pipeline
    m_ColouredTrianglePipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);

    //clear the shader stages for the builder
    pipelineBuilder.shaderStages.clear();

    //add the other shaders
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, redTriangleVertexShader));

    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, redTriangleFragShader));

    //build the red triangle pipeline
    m_RedTrianglePipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);

    //build the mesh pipeline
    //connect the pipeline builder vertex input info to the one we get from Vertex
    Fish::VertexInputDescription vertexDescription = Fish::Vertex::get_vertex_description();
    pipelineBuilder.vertexInputState.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.vertexInputState.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
    pipelineBuilder.vertexInputState.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.vertexInputState.vertexBindingDescriptionCount = vertexDescription.bindings.size();

    //clear the shader stages for the builder
    pipelineBuilder.shaderStages.clear();

    //add the other shaders
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleMeshShader));

    //make sure that triangleFragShader is holding the compiled colored_triangle.frag
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

    //build the mesh triangle pipeline
    m_MeshPipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);

    //destroy all shader modules, outside of the queue
    vkDestroyShaderModule(m_Device, redTriangleVertexShader, nullptr);
    vkDestroyShaderModule(m_Device, redTriangleFragShader, nullptr);
    vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
    vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);
    vkDestroyShaderModule(m_Device, triangleMeshShader, nullptr);

    deletionQueue.push_function([=]() {
        //destroy the 2 pipelines we have created
        vkDestroyPipeline(m_Device, m_RedTrianglePipeline, nullptr);
        vkDestroyPipeline(m_Device, m_ColouredTrianglePipeline, nullptr);
        vkDestroyPipeline(m_Device, m_MeshPipeline, nullptr);

        //destroy the pipeline layout that they use
        vkDestroyPipelineLayout(m_Device, m_TrianglePipelineLayout, nullptr);
    });
}

void VulkanEngine::init_synchronisation_structures()
{
    //create synchronization structures

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;

    //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &renderFence));

    //for the semaphores we don't need any flags
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &presentSemaphore));
    VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &renderSemaphore));

    deletionQueue.push_function([=]() {
        vkDestroySemaphore(m_Device, presentSemaphore, nullptr);
        vkDestroySemaphore(m_Device, renderSemaphore, nullptr);
    });
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

    deletionQueue.push_function([=]() { vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr); });
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
    
    const unsigned int timeout = 1000000000;

    if (m_IsInitialized) {

        //make sure the GPU has stopped doing its things
        vkWaitForFences(m_Device, 1, &renderFence, true, timeout);

        deletionQueue.flush();

        vkDestroyDevice(m_Device, nullptr);
        
        vkDestroySurfaceKHR(m_VkInstance, m_SurfaceKHR, nullptr);

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
    VK_CHECK(vkWaitForFences(m_Device, 1, &renderFence, true, timeout));
    VK_CHECK(vkResetFences(m_Device, 1, &renderFence));

    //request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, timeout, presentSemaphore, nullptr, &swapchainImageIndex));


    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // get local frame command buffer
    VkCommandBuffer cmd = commandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext = nullptr;

    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearValue clearValue;
    float framePeriod = 120.f;
    float flash = std::abs(std::sin(m_FrameNumber / framePeriod));
    clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

    //start the main renderpass.
    //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.pNext = nullptr;

    rpInfo.renderPass = m_MainRenderPass;
    rpInfo.renderArea.offset.x = 0;
    rpInfo.renderArea.offset.y = 0;
    rpInfo.renderArea.extent = m_WindowExtents;
    rpInfo.framebuffer = m_FrameBuffers[swapchainImageIndex];

    //connect clear values
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Actually send out rendering commands:
    // 
    // 
    
    /*if (m_SelectedShader == 0)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ColouredTrianglePipeline);
    }
    else
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RedTrianglePipeline);
    }

    vkCmdDraw(cmd, 3, 1, 0, 0);*/

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipeline);

    //bind the mesh vertex buffer with offset 0
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_Mesh.vertexBuffer.buffer, &offset);

    //we can now draw the mesh
    vkCmdDraw(cmd, m_Mesh.vertices.size(), 1, 0, 0);


    //finalize the render pass
    vkCmdEndRenderPass(cmd);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &waitStage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &presentSemaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderSemaphore;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submit, renderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &renderSemaphore;
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
            if (e.type == SDL_QUIT) {
                bQuit = true;
            }

            // Quick example of getting a keypress event and logging it.
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_SPACE:
                    m_SelectedShader += 1;
                    if (m_SelectedShader > 1) {
                        m_SelectedShader = 0;
                    }
                    fmt::println("Toggle next shader: ", m_SelectedShader);
                    break;
                }

            }
            
            else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    m_StopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    m_StopRendering = false;
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

void VulkanEngine::load_meshes()
{
    //make the array 3 vertices long
    m_Mesh.vertices.resize(3);

    //vertex positions
    m_Mesh.vertices[0].position = { 1.f, 1.f, 0.0f };
    m_Mesh.vertices[1].position = { -1.f, 1.f, 0.0f };
    m_Mesh.vertices[2].position = { 0.f,-1.f, 0.0f };

    //vertex colors, all green
    m_Mesh.vertices[0].colour = { 0.f, 1.f, 0.0f }; //pure green
    m_Mesh.vertices[1].colour = { 0.f, 1.f, 0.0f }; //pure green
    m_Mesh.vertices[2].colour = { 0.f, 1.f, 0.0f }; //pure green

    //we don't care about the vertex normals

    upload_mesh(m_Mesh);
}

void VulkanEngine::upload_mesh(Fish::Mesh& mesh)
{
    // allocate a buffer for the data of this mesh.
    // map the memory from the mesh into the newly allocated buffer.

    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    //this is the total size, in bytes, of the buffer we are allocating
    bufferInfo.size = mesh.vertices.size() * sizeof(Fish::Vertex);
    //this buffer is going to be used as a Vertex Buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    //let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaallocInfo, &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    //add the destruction of triangle mesh buffer to the deletion queue
    deletionQueue.push_function([=]() { vmaDestroyBuffer(m_Allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation); });

    //copy vertex data
    void* data;
    vmaMapMemory(m_Allocator, mesh.vertexBuffer.allocation, &data);

    memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Fish::Vertex));

    vmaUnmapMemory(m_Allocator, mesh.vertexBuffer.allocation);
}

bool VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
{
    //open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    //find what the size of the file is by looking up the location of the cursor
    //because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    //spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    //put file cursor at beginning
    file.seekg(0);

    //load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    //now that the file is loaded into the buffer, we can close it
    file.close();

    //create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    //codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    //check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass renderPass)
{
    //make viewport state from our stored viewport and scissor.
    //at the moment we won't support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    //setup dummy color blending. We aren't using transparent objects yet
    //the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colourBlendAttachment;

    //build the actual pipeline
    //we now use all of the info structs we have been writing into into this one to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;

    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampler;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    //it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {

        fmt::println("Failed to create pipeline.");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    else
    {
        return newPipeline;
    }
}
