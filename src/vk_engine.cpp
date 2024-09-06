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

#include <glm/gtx/transform.hpp>

#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>

#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>

#include <fish_pipeline.h>
#include <vk_pipelines.h>

#include <fish_logger.h>

// We set a *global* pointer for the vulkan engine singleton reference. 
// We do that instead of a typical singleton because we want to control explicitly when is the class initalized and destroyed. 
// The normal Cpp singleton pattern doesnt give us control over that.
FishEngine* loadedEngine = nullptr;

FishEngine& FishEngine::Get() { return *loadedEngine; }

void FishEngine::init()
{
    FISH_LOG("Initialising Fish engine.");

    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    m_pWindow = SDL_CreateWindow("Fish Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_WindowExtents.width, m_WindowExtents.height, window_flags); 

    initialise_vulkan();
    initialise_swapchain();
    initialise_commands();
    initialise_synchronisation_structures();
    initialise_descriptors();
    initialise_pipelines();
    initialise_default_data();
    initialise_camera();
    initialise_renderables();
    initialise_imgui(); // Required to be called after Vulkan initialisation.

    // If there is no scene to load, create a new empty scene and save that.
    currentScene.load();

    // initialise entity component systems
    //init_ecs();
    

    Fish::Timer::EngineTimer _timer;
    m_EngineTimer = _timer;

    // everything went fine
    m_IsInitialized = true;
    FISH_LOG("Engine successfully initialised.");
}

void FishEngine::initialise_vulkan()
{
    FISH_LOG("Initialising Vulkan...");

    // Abstract the creation of a vulkan context.
    vkb::InstanceBuilder builder;

    //make the vulkan instance, with basic debug features
    vkb::Result<vkb::Instance> instance = 
        builder.set_app_name("Untitled Fish Game")
        .set_engine_name("Fish")
        .request_validation_layers(kUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        //.desire_api_version(VKB_VK_API_VERSION_1_3)
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
    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    //vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.runtimeDescriptorArray = true;

    //use vkbootstrap to select a gpu. 
    //We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_instance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(m_SurfaceKHR)
        .select()
        .value();

    //create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };
    
    // enable shader draw parameters
    VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
    shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shader_draw_parameters_features.pNext = nullptr;
    shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;
    vkb::Device vkb_device = deviceBuilder.add_pNext(&shader_draw_parameters_features).build().value();

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
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);

    m_DeletionQueue.push_function([&]() { vmaDestroyAllocator(m_Allocator); });

    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_GPUProperties);
    std::cout << "The GPU has a minimum buffer alignment of " << m_GPUProperties.limits.minUniformBufferOffsetAlignment << std::endl;
}

void FishEngine::initialise_imgui()
{
    FISH_LOG("Initialising ImGui...");

    // 1: Create the descriptor pool for ImGui.

    // Adapted from the ImGui demos, create structures that ImGui wants.
    // The descriptor pool is storing data for 1000 different types of descriptors, 
    // clearly overkill but suitable for our needs.
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &imguiPool));

    // 2: Initialise the ImGui library.

    // Create the core for ImGui to work from.
    ImGui::CreateContext();

    // Hook ImGui, SDL2, and Vulkan to work together.
    ImGui_ImplSDL2_InitForVulkan(m_pWindow);

    // Important: PipelineRenderingCreateInfo is not part of ImGui_ImplVulkan_InitInfo so this is currently not used.
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_SwapchainImageFormat;

    // Initialise ImGui with Vulkan specifically.
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_VkInstance;
    init_info.PhysicalDevice = m_PhysicalDevice;
    init_info.Device = m_Device;
    init_info.Queue = m_GraphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;             
    
    // Need to explicitly enable VK_KHR_dynamic_rendering extension to use this, even for Vulkan 1.3.
    // It is important to set UseDynamicRendering to true, and ColorAttachmentFormat to our swapchain format
    // because we won't be using Vulkan render_passes but dynamic rendering instead. Unlike the compute shader,
    // we are drawing ImGui directly into the swapchain. This is so we can make more changes during runtime.
    init_info.UseDynamicRendering = true;                     
    init_info.ColorAttachmentFormat = m_SwapchainImageFormat;    

    // Important: This function asks for a VkRenderPass parameter that we do not have and will not use with the Vulkan 1.3 API.
    // Within the body of this function, the only time that the render_pass parameter is used is if init_info.UseDynamicRendering is
    // set to false. If this function fails, first ensure that this value is set to True.
    ImGui_ImplVulkan_Init(&init_info, nullptr); 

    // Immediately submit a GPU command which will upload our ImGui font textures.
    // Calling this function outside of the immediate_submit13(...) function will cause ImGui
    // to not actually render. I spent some time messing around with this before I realised it wasn't
    // stated in the tutorial to do this. 
    VkCommandBuffer cmd = get_current_frame().commandBuffer;
    immediate_submit13([=](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    // Ensure to delete ImGui related memory.
    m_DeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(m_Device, imguiPool, nullptr);
    });
}

void FishEngine::initialise_default_data()
{
    FISH_LOG("Initialising default data...");

    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = { 0.5,-0.5, 0 };
    rect_vertices[1].position = { 0.5,0.5, 0 };
    rect_vertices[2].position = { -0.5,-0.5, 0 };
    rect_vertices[3].position = { -0.5,0.5, 0 };

    rect_vertices[0].color = { 0,0, 0,1 };
    rect_vertices[1].color = { 0.5,0.5,0.5 ,1 };
    rect_vertices[2].color = { 1,0, 0,1 };
    rect_vertices[3].color = { 0,1, 0,1 };

    rect_vertices[0].uv_x = 1;
    rect_vertices[0].uv_y = 0;
    rect_vertices[1].uv_x = 0;
    rect_vertices[1].uv_y = 0;
    rect_vertices[2].uv_x = 1;
    rect_vertices[2].uv_y = 1;
    rect_vertices[3].uv_x = 0;
    rect_vertices[3].uv_y = 1;

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle = upload_mesh(rect_indices, rect_vertices);

    //3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    //checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    _errorCheckerboardImage = create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(m_Device, &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_Device, &sampl, nullptr, &_defaultSamplerLinear);
}

void FishEngine::initialise_camera()
{
    FISH_LOG("Initialising camera...");

    // Zero-out/Initialise the camera's default data.

    currentScene.camera.m_Velocity = glm::vec3(0.f);
    currentScene.camera.m_Position = glm::vec3(28.0f, 22.0f, 21.0f);
    currentScene.camera.m_Pitch = -0.3;
    currentScene.camera.m_Yaw = 5.6;
}

void FishEngine::initialise_renderables()
{
    FISH_LOG("Initialising renderables...");

    // Todo:
    // Iterate the entire assets directory and load everything inside of it in a function.
    // For now, we will load only the things we explicitly want but this will eventually become quite tedious.

    {
        std::string structurePath = { "../../assets/PolyPizza/Trampoline.glb" };
        auto structureFile = Fish::Loader::loadGltf(this, structurePath);
        assert(structureFile.has_value());
        currentScene.loadedScenes[Fish::Utility::extract_file_name(structurePath).c_str()] = *structureFile;
    }
    {
        std::string structurePath = { "../../assets/house.glb" };
        auto structureFile = Fish::Loader::loadGltf(this, structurePath);
        assert(structureFile.has_value());
        currentScene.loadedScenes[Fish::Utility::extract_file_name(structurePath).c_str()] = *structureFile;
    }
    {
        std::string structurePath = { "../../assets/PolyPizza/BasicCar.glb" };
        auto structureFile = Fish::Loader::loadGltf(this, structurePath);
        assert(structureFile.has_value());
        currentScene.loadedScenes[Fish::Utility::extract_file_name(structurePath).c_str()] = *structureFile;
    }
}

void FishEngine::initialise_swapchain()
{
    FISH_LOG("Initialising swapchain...");

    create_swapchain(m_WindowExtents.width, m_WindowExtents.height);

    //depth image size will match the window
    VkExtent3D drawImageExtent = {
        m_WindowExtents.width,
        m_WindowExtents.height,
        1
    };

    //hardcoding the depth format to 32 bit float
    m_DrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_DrawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(m_DrawImage.imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(m_Allocator, &rimg_info, &rimg_allocinfo, &m_DrawImage.image, &m_DrawImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(m_DrawImage.imageFormat, m_DrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(m_Device, &rview_info, nullptr, &m_DrawImage.imageView));

    //> depthimg
    m_DepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_DepthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(m_DepthImage.imageFormat, depthImageUsages, drawImageExtent);

    //allocate and create the image
    vmaCreateImage(m_Allocator, &dimg_info, &rimg_allocinfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(m_DepthImage.imageFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(m_Device, &dview_info, nullptr, &m_DepthImage.imageView));
    //< depthimg
        //add to deletion queues
    m_DeletionQueue.push_function([=]() {
        vkDestroyImageView(m_Device, m_DrawImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);

        vkDestroyImageView(m_Device, m_DepthImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
    });
}

void FishEngine::initialise_commands()
{
    FISH_LOG("Initialising commands...");

    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));

        //allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_Frames[i].commandBuffer));
                
        m_DeletionQueue.push_function([=]() { vkDestroyCommandPool(m_Device, m_Frames[i].commandPool, nullptr); });
    }    

    // > Immediate Commands
    VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_ImmediateCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo immediateCmdAllocInfo = vkinit::command_buffer_allocate_info(m_ImmediateCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_Device, &immediateCmdAllocInfo, &m_ImmediateCommandBuffer));

    m_DeletionQueue.push_function([=]() { vkDestroyCommandPool(m_Device, m_ImmediateCommandPool, nullptr); });
    // < Immediate Commands
}

void FishEngine::initialise_pipelines()
{
    FISH_LOG("Initialising pipelines...");

    // Compute Pipelines.
    init_background_pipelines();

    // Graphics Pipelines.
    //init_triangle_pipeline();
    //init_mesh_pipeline();

    metalRoughMaterial.build_pipelines(this);
}

void FishEngine::init_background_pipelines()
{
    // Create the main background effect pipeline layout.

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;
    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device, &computeLayout, nullptr, &m_GradientPipelineLayout));

    // Load any shader files we want to use.

    VkShaderModule gradientColourShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/gradient_color.comp.spv", m_Device, &gradientColourShader))
    {
        fmt::print("Error when building the gradient colour shader \n");
    }

    VkShaderModule gradientShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/gradient.comp.spv", m_Device, &gradientShader))
    {
        fmt::print("Error when building the compute/gradient shader \n");
    }
    
    VkShaderModule skyShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/sky.comp.spv", m_Device, &skyShader))
    {
        fmt::print("Error when building the sky shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = m_GradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    // Create an effect for our background, toggleable in ImGui.
    ComputeEffect gradient;
    gradient.layout = m_GradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);
    VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));
    
    // Change the shader module only to create sky shader.
    // Not too sure why we do this.
    computePipelineCreateInfo.stage.module = skyShader;

    // Create a secondary effect for our background, toggleable in ImGui.
    ComputeEffect sky;
    sky.layout = m_GradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
    VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    computePipelineCreateInfo.stage.module = gradientColourShader;

    // Create another effect for our background, toggleable in ImGui.
    ComputeEffect gradientColour;
    gradientColour.layout = m_GradientPipelineLayout;
    gradientColour.name = "gradientColour";
    gradientColour.data = {};
    gradientColour.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
    VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientColour.pipeline));

    // Make sure we add our effects into the vector to be accessed by ImGui.
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);
    backgroundEffects.push_back(gradientColour);

    // Ensure any allocated memory is deleted.
    vkDestroyShaderModule(m_Device, gradientColourShader, nullptr);
    vkDestroyShaderModule(m_Device, gradientShader, nullptr);
    vkDestroyShaderModule(m_Device, skyShader, nullptr);
    m_DeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(m_Device, m_GradientPipelineLayout, nullptr);
        vkDestroyPipeline(m_Device, gradientColour.pipeline, nullptr);
        vkDestroyPipeline(m_Device, sky.pipeline, nullptr);
        vkDestroyPipeline(m_Device, gradient.pipeline, nullptr);
    });
}

//void FishVulkanEngine::init_triangle_pipeline()
//{
//    VkShaderModule triangleFragShader;
//    if (!vkutil::load_shader_module("../../shaders/colored_triangle.frag.spv", m_Device, &triangleFragShader)) {
//        fmt::print("Error when building the triangle fragment shader module");
//    }
//    else {
//        fmt::print("Triangle fragment shader succesfully loaded");
//    }
//
//    VkShaderModule triangleVertexShader;
//    if (!vkutil::load_shader_module("../../shaders/colored_triangle.vert.spv", m_Device, &triangleVertexShader)) {
//        fmt::print("Error when building the triangle vertex shader module");
//    }
//    else {
//        fmt::print("Triangle vertex shader succesfully loaded");
//    }
//
//    //build the pipeline layout that controls the inputs/outputs of the shader
//    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
//    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
//    VK_CHECK(vkCreatePipelineLayout(m_Device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));
//
//    // Create the pipeline using our builder abstraction.
//
//    Fish::PipelineBuilder13 pipelineBuilder;
//
//    //use the triangle layout we created
//    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
//    //connecting the vertex and pixel shaders to the pipeline
//    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
//    //it will draw triangles
//    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
//    //filled triangles
//    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
//    //no backface culling
//    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
//    //no multisampling
//    pipelineBuilder.set_multisampling_none();
//    //no blending
//    pipelineBuilder.disable_blending();
//    //no depth testing
//    pipelineBuilder.disable_depthtest();
//
//    //connect the image format we will draw into, from draw image
//    pipelineBuilder.set_color_attachment_format(m_DrawImage.imageFormat);
//    pipelineBuilder.set_depth_format(m_DepthImage.imageFormat);
//
//    //finally build the pipeline
//    _trianglePipeline = pipelineBuilder.build_pipeline(m_Device);
//
//    //clean structures
//    vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
//    vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);
//
//    m_DeletionQueue.push_function([&]() {
//        vkDestroyPipelineLayout(m_Device, _trianglePipelineLayout, nullptr);
//        vkDestroyPipeline(m_Device, _trianglePipeline, nullptr);
//    });
//}

void FishEngine::init_mesh_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/tex_image.frag.spv", m_Device, &triangleFragShader)) {
        fmt::print("Error when building the triangle fragment shader module");
    }
    else {
        fmt::print("Coloured triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/colored_triangle_mesh.vert.spv", m_Device, &triangleVertexShader)) {
        fmt::print("Error when building the triangle vertex shader module");
    }
    else {
        fmt::print("Coloured triangle mesh vertex shader succesfully loaded");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

    Fish::Pipeline::Builder pipelineBuilder;

    //use the triangle layout we created
    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    //it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //filled triangles
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    //no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    //no multisampling
    pipelineBuilder.set_multisampling_none();
    //no blending
    pipelineBuilder.disable_blending();
    //additive blending
    //pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(m_DrawImage.imageFormat);
    pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

    //finally build the pipeline
    _meshPipeline = pipelineBuilder.build_pipeline(m_Device);

    //clean structures
    vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
    vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);

    m_DeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(m_Device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(m_Device, _meshPipeline, nullptr);
    });
}

void FishEngine::initialise_synchronisation_structures()
{
    FISH_LOG("Initialising sync structures...");

    //create synchronization structures

    //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    //for the semaphores we don't need any flags
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_Frames[i].renderFence));

        // queue destruction of fences.
        m_DeletionQueue.push_function([=]() { 
            vkDestroyFence(m_Device, m_Frames[i].renderFence, nullptr); 
        });

        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].presentSemaphore));
        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].renderSemaphore));

        // queue destruction of semaphores.
        m_DeletionQueue.push_function([=]() {
            vkDestroySemaphore(m_Device, m_Frames[i].presentSemaphore, nullptr);
            vkDestroySemaphore(m_Device, m_Frames[i].renderSemaphore, nullptr);
        });
    }    

    // > Immediate Fence
    VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_ImmediateFence));
    m_DeletionQueue.push_function([=]() { vkDestroyFence(m_Device, m_ImmediateFence, nullptr); });
    // < Immediate Fence
}

void FishEngine::create_swapchain(uint32_t width, uint32_t height)
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

void FishEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < m_SwapchainImageViews.size(); i++) {

        vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
    }
}

void FishEngine::rebuild_swapchain()
{
    vkQueueWaitIdle(m_GraphicsQueue);

    vkb::SwapchainBuilder swapchainBuilder { m_PhysicalDevice, m_Device, m_SurfaceKHR };

    SDL_GetWindowSizeInPixels(m_pWindow, (int*)&m_WindowExtents.width, (int*)&m_WindowExtents.height);

    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    vkDestroyImageView(m_Device, m_DrawImage.imageView, nullptr);
    vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .use_default_format_selection()
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(m_WindowExtents.width, m_WindowExtents.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    //store swapchain and its related images
    m_Swapchain = vkbSwapchain.swapchain;
    m_SwapchainImages = vkbSwapchain.get_images().value();
    m_SwapchainImageViews = vkbSwapchain.get_image_views().value();
    m_SwapchainImageFormat = vkbSwapchain.image_format;

    //depth image size will match the window
    VkExtent3D drawImageExtent = { m_WindowExtents.width, m_WindowExtents.height, 1 };

    //hardcoding the depth format to 32 bit float
    m_DrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(m_DrawImage.imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(m_Allocator, &rimg_info, &rimg_allocinfo, &m_DrawImage.image, &m_DrawImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(m_DrawImage.imageFormat, m_DrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(m_Device, &rview_info, nullptr, &m_DrawImage.imageView));

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = m_DrawImage.imageView;

    VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_DrawImageDescriptors, &imgInfo, 0);

    vkUpdateDescriptorSets(m_Device, 1, &cameraWrite, 0, nullptr);

    //add to deletion queues
    m_DeletionQueue.push_function([&]() {
        vkDestroyImageView(m_Device, m_DrawImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);
    });
}

void FishEngine::resize_swapchain()
{
    // Wait for all GPU commands to finish.
    vkDeviceWaitIdle(m_Device);

    // Make sure to destroy the swapchain before creating a new one.
    destroy_swapchain();

    // Get the new window dimensions after resize.
    int w, h;
    SDL_GetWindowSize(m_pWindow, &w, &h);
    m_WindowExtents.width = w;
    m_WindowExtents.height = h;

    // Create a new swapchain with those window dimensions.
    create_swapchain(m_WindowExtents.width, m_WindowExtents.height);

    // Now that the new swapchain has been created, we no longer need this flag set.
    swapchain_resize_requested = false;
}

void FishEngine::initialise_descriptors()
{
    FISH_LOG("Initialising descriptors...");

    // initialize the descriptor allocator with 10 sets, and 
    // 1 descriptor per set of type VK_DESCRIPTOR_TYPE_STORAGE_IMAGE. 
    // Thats the type used for a image that can be written to from a compute shader.
    // use the layout builder to build the descriptor set layout we need, which is 
    // a layout with only 1 single binding at binding number 0, of type 
    // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE too (matching the pool).

    //create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
    };

    m_GlobalDescriptorAllocator.init_pool(m_Device, 10, sizes);
    m_DeletionQueue.push_function(
        [&]() { vkDestroyDescriptorPool(m_Device, m_GlobalDescriptorAllocator.pool, nullptr); });

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        m_DrawImageDescriptorLayout = builder.build(m_Device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        currentScene._gpuSceneDataDescriptorLayout = builder.build(m_Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    m_DeletionQueue.push_function([&]() {
        vkDestroyDescriptorSetLayout(m_Device, m_DrawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_Device, currentScene._gpuSceneDataDescriptorLayout, nullptr);
    });

    m_DrawImageDescriptors = m_GlobalDescriptorAllocator.allocate(m_Device, m_DrawImageDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_image(0, m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.update_set(m_Device, m_DrawImageDescriptors);
    }
    for (int i = 0; i < kFrameOverlap; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        m_Frames[i].frameDescriptors = DescriptorAllocatorGrowable{};
        m_Frames[i].frameDescriptors.init(m_Device, 1000, frame_sizes);
        m_DeletionQueue.push_function([&, i]() {
            m_Frames[i].frameDescriptors.destroy_pools(m_Device);
        });
    }
}

void FishEngine::cleanup()
{
    // Delete in the opposite order to what they were created, to avoid dependency errors.
    
    const unsigned int timeout = 1000000000;

    if (m_IsInitialized) {

        //make sure the GPU has stopped doing its things
        vkWaitForFences(m_Device, 1, &get_current_frame().renderFence, true, timeout);
        vkDeviceWaitIdle(m_Device);

        currentScene.loadedScenes.clear();
                
        for (auto& frame : m_Frames) {
            frame.deletionQueue.flush();
        }

        m_DeletionQueue.flush();

        destroy_swapchain();

        vkDestroySurfaceKHR(m_VkInstance, m_SurfaceKHR, nullptr);

        vmaDestroyAllocator(m_Allocator);

        vkDestroyDevice(m_Device, nullptr);

        vkb::destroy_debug_utils_messenger(m_VkInstance, m_DebugMessenger);

        vkDestroyInstance(m_VkInstance, nullptr);

        SDL_DestroyWindow(m_pWindow);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void FishEngine::prepare_imgui()
{
    // Connect backend ImGui functionality specifically for Vulkan and SDL2 and create all 
    // basic ImGui functionality so that the demo window or custom windows are able to be rendered and interacted with.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(m_pWindow);
    ImGui::NewFrame();

    // Create/Begin ImGui render data. We don't actually render anything here, we just create all the 
    // necessary information so that within our regular render loop we can read from ImGui::GetDrawData() 
    // to get all data that we have defined here.
    create_imgui_draw_data();

    // Prepare the data for rendering so you can call ImGui::GetDrawData()
    ImGui::Render();
}

void FishEngine::create_imgui_draw_data()
{
    // Begin Debug Overlay
    {
        ImGui::Begin("Debug Overlay", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar);
        imgui_debug_data();
        ImGui::End(); 
    }
    // End Debug Overlay

    // Begin Scene Hierarchy
    {
        ImGui::Begin("Scene Hierarchy", (bool*)0, ImGuiWindowFlags_NoMove);
        imgui_scene_hierarchy();
        ImGui::End();
    }
    // End Scene Hierarchy

    // Begin Util Buttons
    {
        ImGui::Begin("Utility Buttons", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
        imgui_util_buttons();
        ImGui::End();
        
    }
    // End Util Buttons

    // Begin Background Effects
    {
        /*if (ImGui::Begin("Background")) {

            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected effect: ", selected.name);

            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();*/
    }
    // End Background Effects

    // Begin render scale slider
    {
        /*if (ImGui::Begin("Zoom")) {
            ImGui::SliderFloat("Zoom factor", &renderScale, 0.3f, 1.f);
        }
        ImGui::End();*/
    }
    // End render scale slider
}

void FishEngine::imgui_debug_data()
{
    ImGui::Text("Debug data for Fish engine.");

    ImGui::Text("Elapsed: %fs", stats.total_elapsed);
    ImGui::Text("Frame Time: %fms", stats.frame_time); 
    ImGui::Text("Geometry Draw Time: %fms", stats.geometry_draw_time);
    ImGui::Text("Scene Update Time: %fms", stats.scene_update_time);
    ImGui::Text("Number of triangles: %i", stats.triangle_count);
    ImGui::Text("Number of draw calls: %i", stats.drawcall_count);
    ImGui::Text("Camera Position: %f, %f, %f", stats.camera_position.x, stats.camera_position.y, stats.camera_position.z);
    ImGui::Text("Camera Pitch/Yaw: %f/%f", stats.camera_pitch, stats.camera_yaw);
}

void FishEngine::imgui_scene_hierarchy()
{
    std::string sceneNameText = "Scene Name: " + currentScene.sceneName;
    ImGui::Text(sceneNameText.c_str());
    ImGui::NewLine();

    int index = 0;
    for (auto& object : currentScene.loadedScenes)
    {
        // Shared ptr must be dereferenced into a reference (not sure why I struggled here).
        Fish::Loader::LoadedGLTF& obj = *object.second;

        if (ImGui::TreeNode(object.first.c_str())) {
            ImGui::NewLine();

            // Local cache, with thanks to MellOH for the help.
            float position[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
            float rotation[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
            float scale[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };

            position[0] = obj.transform.position.x;
            position[1] = obj.transform.position.y;
            position[2] = obj.transform.position.z;
            rotation[0] = obj.transform.rotation.x;
            rotation[1] = obj.transform.rotation.y;
            rotation[2] = obj.transform.rotation.z;
            scale[0] = obj.transform.scale.x;
            scale[1] = obj.transform.scale.y;
            scale[2] = obj.transform.scale.z;

            ImGui::DragFloat3("Position", position);
            ImGui::DragFloat3("Rotation", rotation);
            ImGui::DragFloat3("Scale", scale);

            obj.transform.position.x = position[0];
            obj.transform.position.y = position[1];
            obj.transform.position.z = position[2];
            obj.transform.rotation.x = rotation[0];
            obj.transform.rotation.y = rotation[1];
            obj.transform.rotation.z = rotation[2];
            obj.transform.scale.x = scale[0];      
            obj.transform.scale.y = scale[1];      
            obj.transform.scale.z = scale[2];      

            glm::vec3 t(position[0], position[1], position[2]);                             // Translation.
            glm::mat4 tm = glm::translate(glm::mat4(1.0f), t);                              // Translation.
            glm::vec3 rx(1.0f, 0.0f, 0.0f);                                                 // Rotation.
            glm::vec3 ry(0.0f, 1.0f, 0.0f);                                                 // Rotation.
            glm::vec3 rz(0.0f, 0.0f, 1.0f);                                                 // Rotation.
            glm::mat4 rxm = glm::rotate(glm::mat4(1.0f), glm::radians(rotation[0]), rx);    // Rotation.
            glm::mat4 rym = glm::rotate(glm::mat4(1.0f), glm::radians(rotation[1]), ry);    // Rotation.
            glm::mat4 rzm = glm::rotate(glm::mat4(1.0f), glm::radians(rotation[2]), rz);    // Rotation.
            glm::mat4 rm = rzm * rym * rxm;                                                 // Rotation.
            glm::vec3 s(scale[0], scale[1], scale[2]);                                      // Scale.
            glm::mat4 sm = glm::scale(glm::mat4(1.0f), s);                                  // Scale.
            glm::mat4 _final = tm * rm * sm;                                                // Combined.

            // Set all node transforms. All children nodes must be multiplied by the new parent/world transformation matrix.
            for (auto& n : obj.topNodes) {
                Node& _n = *n;
                for (auto& c : _n.children) {
                    Node& _c = *c;

                    _c.localTransformMatrix *= _final;
                }
                _n.worldTransformMatrix = _final;
            }
            ImGui::TreePop();
        }
        ++index;
    }
}

void FishEngine::imgui_util_buttons()
{
    ImVec2 defaultButtonSize = ImVec2(100.f, 25.f);

    if (ImGui::Button("Save Scene", defaultButtonSize))
    {
        FISH_LOG("Saving scene...");
        currentScene.save();
    }
    if (ImGui::Button("Load Scene", defaultButtonSize))
    {
        FISH_LOG("Loading scene...");
        currentScene.load();
    }
    if (ImGui::Button("New Scene", defaultButtonSize))
    {
        currentScene.create_new();
    }
    if (ImGui::Button("Rename scene", defaultButtonSize)) 
    {
        ImGui::OpenPopup("Rename Scene");
    }
    rename_scene_modal();
    
}

void FishEngine::rename_scene_modal()
{
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Rename Scene", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char str0[128] = "Hello, world!";
        ImGui::InputText("input text", str0, IM_ARRAYSIZE(str0));

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            currentScene.sceneName = str0;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

//void FishVulkanEngine::imgui_scene_data()
//{
//    // TODO: Implement the button functionality for the below buttons.
//
//    ImVec2 defaultButtonSize = ImVec2(100.0f, 25.0f);
//
//    if (ImGui::Button("New Scene", defaultButtonSize)) {
//        std::cout << "\n\nCreating new scene...\n\n";
//    }
//
//    if (ImGui::Button("Save Scene", defaultButtonSize)) {
//        std::cout << "\n\nSaving scene...\n\n";
//    }
//
//    if (ImGui::Button("Load Scene", defaultButtonSize)) {
//        std::cout << "\n\nLoading scene...\n\n";
//    }
//
//    ImGui::Text("Objects in scene: %i", Fish::ResourceManager::Get().m_Scene.m_SceneObjects.size()); // Must be integer to work.
//}

GPUMeshBuffers FishEngine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    //create vertex buffer
    newSurface.vertexBuffer = create_buffer13(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_Device, &deviceAdressInfo);

    //create index buffer
    newSurface.indexBuffer = create_buffer13(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer13 staging = create_buffer13(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex and index buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit13([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
}

void FishEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(m_SwapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void FishEngine::draw()
{
    // Draw all objects within the loadedScenes container.
    for (const auto& instance : currentScene.loadedScenes) {
        currentScene.loadedScenes[instance.first]->Draw(glm::mat4{ 1.0f }, mainDrawContext);
    }

    // Wait for the render fence to enter a signalled state meaning the GPU has finished 
    // rendering the previous frame and the GPU is now synchronised with the CPU.
    const unsigned int timeout = 1000000000;
    VK_CHECK(vkWaitForFences(m_Device, 1, &get_current_frame().renderFence, true, timeout));

    // Clear all per-frame memory allocation.
    get_current_frame().deletionQueue.flush();
    get_current_frame().frameDescriptors.clear_pools(m_Device);

    // The swapchain image index from our different images that we want to request for the next frame.
    uint32_t swapchainImageIndex;

    // Request the next swapchain image.
    VkResult e = vkAcquireNextImageKHR(m_Device, m_Swapchain, timeout, get_current_frame().presentSemaphore, nullptr, &swapchainImageIndex);

    // If the swapchain image that we received is out of date, it will need to be reconstructed.
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_resize_requested = true;
        return;
    }

    // Calculate the draw extents for our swapchain image using our render scale factor used for dynamic resolution.
    m_DrawExtent.width = std::min(m_SwapchainExtent.width, m_DrawImage.imageExtent.width) * renderScale;
    m_DrawExtent.height = std::min(m_SwapchainExtent.height, m_DrawImage.imageExtent.height) * renderScale;;

    // Set the state of the render fence to be unsignaled, we already waited for the fence synchronisation so now
    // we want to make sure it's ready to be used again this frame. You can't use the same fence on multiple
    // GPU commands without resetting it at some point.
    VK_CHECK(vkResetFences(m_Device, 1, &get_current_frame().renderFence));

    // Since everything is synchronised and the previous frame is not executing any more commands, we can safely
    // reset our command buffer so that we can begin recording commands for this frame.
    VK_CHECK(vkResetCommandBuffer(get_current_frame().commandBuffer, 0));

    // The current frame's command buffer in shorthand for easier writing.
    VkCommandBuffer cmd = get_current_frame().commandBuffer;

    // Configure the settings for how we want to begin recording this frame's command buffer. 
    // We only use this command buffer once so we need to let Vulkan know that.
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Open the command buffer so we can record commands into it before submitting. This will set the command buffer to be put
    // into a recording state.
    // Commands in Vulkan are not executed directly using function calls. We need to record all of the operations we 
    // want to perform into a command buffer object and submit all of those commands together so that Vulkan can 
    // efficiently process all of them at once. This allows command recording to happen over multiple threads.
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Send our current draw image to be transitioned into a general layout, so that we can write into it.
    // We don't care about the previous layout since we are going to overwrite it.
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // Render our clear colour, or set our background effects.
    draw_main(cmd);

    // Transition the draw and swapchain image into their optimal transfer layouts.
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    VkExtent2D extent;
    extent.height = m_WindowExtents.height;
    extent.width = m_WindowExtents.width;

    // Copy our draw image into the swapchain as this is what will eventually be presented to the screen.
    vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_SwapchainExtent);

    // Transition the swapchain image into an optimal layout, so that we can write to it.
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Render our ImGui data into the swapchain.
    draw_imgui(cmd, m_SwapchainImageViews[swapchainImageIndex]);

    // Transition the swapchain image into a present layout so that we can draw/present it on the screen.
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Close the command buffer so no more commands can be recorded into it. This will set the command buffer to be put into an executable state.
    // Any errors that happen during recording will be caught here, and the command buffer will be put into an invalid state.
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the command buffer submission to the graphics queue.
    // The command buffer should now be in an executable state with all our desired commands ready to be submitted. Our swapchain image has been drawn to and
    // we want to prepare to submit this to the graphics queue so when the swapchain is flipped, this frame's image can be drawn to the screen.
    // We need to wait for the present semaphore so we know our swapchain is ready. And we signal our render semaphore to alert our rendering has finished.

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().presentSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);
    VkSubmitInfo2 submit = vkinit::submit_info2(&cmdinfo, &signalInfo, &waitInfo);

    // Submit our command buffer to the graphics queue. Our render fence will be signaled once all submitted command buffers have been executed.
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, get_current_frame().renderFence));

    // Prepare presentation of the swapchain that we rendered into to be displayed to our screen.
    // We need to wait on the render semaphore to signal that vkQueueSubmit2 executed properly before we can issue the present request.
    // It is necessary that all drawing commands have finished before the image can be displayed.

    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    // Send all present information to be queued for display. It's only added to a queue here because we are using double buffering so 
    // the previous swapchain image will be swapped with this frame's swapchain image.
    VkResult presentResult = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

    // If the present command saw that our swapchain needs to be resized, set the flag so this can be done.
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_resize_requested = true;
    }

    // This frame is now completed, so we can increment total frames passed.
    m_FrameNumber++;
}

void FishEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    m_EngineTimer.reset();

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

            // Key down events.
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_SPACE:
                    break;
                }
            }
            
            // Window events.
            else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    swapchain_resize_requested = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    m_StopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    m_StopRendering = false;
                }
            }

            // Have the camera process the events.
            currentScene.camera.processSDLEvent(e);

            // Have ImGui process the events.
            ImGui_ImplSDL2_ProcessEvent(&e);

        } // End event loop.

        // do not draw.
        if (m_StopRendering) continue;

        // if window has been resized in any way, the swapchain needs to be resized to render images to our new window.
        if (swapchain_resize_requested) {
            resize_swapchain();
        }

        m_EngineTimer.tick();
        stats.total_elapsed = m_EngineTimer.engine_time();
        stats.frame_time = m_EngineTimer.frame_time();        

        // Set all draw data for ImGui so that the render loop can submit this data.
        prepare_imgui();  

        auto scene_start = std::chrono::system_clock::now();                                                    // Start the timer for the scene update function.
        
        update();                                                                                         // Update the scene.

        auto scene_end = std::chrono::system_clock::now();                                                      // End the timer for the scene update function.
        auto scene_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(scene_end - scene_start);    // Calculate the update time based on start and end time.
        stats.scene_update_time = scene_elapsed.count() / 1000.f;

        // Main draw loop.
        draw();
    }
}

bool is_visible(const Fish::Resource::RenderObject& obj, const glm::mat4& viewproj) {
    std::array<glm::vec3, 8> corners{
        glm::vec3 { 1, 1, 1 },
        glm::vec3 { 1, 1, -1 },
        glm::vec3 { 1, -1, 1 },
        glm::vec3 { 1, -1, -1 },
        glm::vec3 { -1, 1, 1 },
        glm::vec3 { -1, 1, -1 },
        glm::vec3 { -1, -1, 1 },
        glm::vec3 { -1, -1, -1 },
    };

    glm::mat4 matrix = viewproj * obj.transformationMatrix;

    glm::vec3 min = { 1.5, 1.5, 1.5 };
    glm::vec3 max = { -1.5, -1.5, -1.5 };

    for (int c = 0; c < 8; c++) {
        // project each corner into clip space
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
        max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    }
    else {
        return true;
    }
}

void FishEngine::draw_geometry(VkCommandBuffer cmd)
{
    // Reset our count for this frame.
    stats.drawcall_count = 0;
    stats.triangle_count = 0;

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());

    for (int i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
        if (is_visible(mainDrawContext.OpaqueSurfaces[i], currentScene.sceneData.viewproj)) {
            opaque_draws.push_back(i);
        }
    }

    // sort the opaque surfaces by material and mesh
    std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
        const Fish::Resource::RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
    const Fish::Resource::RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
    if (A.material == B.material) {
        return A.indexBuffer < B.indexBuffer;
    }
    else {
        return A.material < B.material;
    }
        });

    //allocate a new uniform buffer for the scene data
    AllocatedBuffer13 gpuSceneDataBuffer = create_buffer13(sizeof(Fish::GPU::GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame().deletionQueue.push_function([=, this]() {
        destroy_buffer(gpuSceneDataBuffer);
    });

    //write the buffer
    Fish::GPU::GPUSceneData* sceneUniformData = (Fish::GPU::GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = currentScene.sceneData;

    //create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = get_current_frame().frameDescriptors.allocate(m_Device, currentScene._gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(Fish::GPU::GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(m_Device, globalDescriptor);

    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    // void draw_single_instance(const Fish::Resource::RenderObject& r);
    auto draw = [&](const Fish::Resource::RenderObject& r) {
        if (r.material != lastMaterial) {
            lastMaterial = r.material;
            if (r.material->pipeline != lastPipeline) {

                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);

                VkViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)m_WindowExtents.width;
                viewport.height = (float)m_WindowExtents.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;
                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = m_WindowExtents.width;
                scissor.extent.height = m_WindowExtents.height;
                vkCmdSetScissor(cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, &r.material->materialSet, 0, nullptr);
        }
        if (r.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = r.indexBuffer;

            if (!r.indexBuffer) FISH_FATAL("RenderObject.indexBuffer failed to read.");
            vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        // calculate final mesh matrix
        GPUDrawPushConstants push_constants = {};
        if (!r.vertexBufferAddress) FISH_FATAL("RenderObject.vertexBufferAddress failed to read.");
        push_constants.worldMatrix = r.transformationMatrix;
        push_constants.vertexBuffer = r.vertexBufferAddress;

        vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
        
        // Count how many draw calls we have done this frame, along with the amount of triangles being rendered.
        stats.drawcall_count++;
        stats.triangle_count += r.indexCount / 3;
    };

    for (auto& r : opaque_draws) {
        draw(mainDrawContext.OpaqueSurfaces[r]);
    }

    for (auto& r : mainDrawContext.TransparentSurfaces) {
        draw(r);
    }

    // we delete the draw commands now that we processed them
    mainDrawContext.OpaqueSurfaces.clear();
    mainDrawContext.TransparentSurfaces.clear();
}

void FishEngine::draw_main(VkCommandBuffer cmd)
{
    // Get the currently selected effect set by the ImGui overlay.
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    // Bind our selected effect's pipeline to the command buffer, configured as a compute pipeline.
    // Compute pipelines handle calculations that can be done in parallel with the graphics pipeline. It also flows data through a programmable compute shader stage.
    // This compute shader stage is not part of the graphics pipeline and they have their own pipeline that runs independently.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // Bind the descriptor set containing the draw image for the compute pipeline.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DrawImageDescriptors, 0, nullptr);

    // Submit the push constants to the GPU to be accessible within the shader.
    vkCmdPushConstants(cmd, m_GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // Execute the compute pipeline dispatch. According to the Vulkan Guide, we are using a 16x16 size workgroup so we need to divide by that.
    vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);

    // Transition our draw image to prepare for geometry rendering.
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(m_WindowExtents, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);

    auto start = std::chrono::system_clock::now(); // Start the geometry draw timer.

    draw_geometry(cmd);

    // End and calculate the geometry draw time.
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.geometry_draw_time = elapsed.count() / 1000.f;

    vkCmdEndRendering(cmd);
}

//void FishVulkanEngine::initialise_entity_component_system()
//{
//    // initalise the coordinator to connect all parts of the ecs
//    m_Ecs->Init();
//
//    // register the components that are going to be used by entities
//    m_Ecs->RegisterComponent<Fish::Component::Transform>();
//    m_Ecs->RegisterComponent<Fish::Component::RigidBody>();
//
//    m_PhysicsSystem = m_Ecs->RegisterSystem<Fish::ECS::System::Physics>();
//
//    // create and set the component signatures so the system knows what components to be updating
//    Fish::ECS::Signature signature;
//    //signature.reset();
//
//    // set up for the physics system
//    signature.set(m_Ecs->GetComponentType<Fish::Component::Transform>());
//    signature.set(m_Ecs->GetComponentType<Fish::Component::RigidBody>());
//    m_Ecs->SetSystemSignature<Fish::ECS::System::Physics>(signature);
//    m_PhysicsSystem->init(m_Ecs);
//
//    // set up for the ... 
//    //signature.reset();
//    // ...
//}

FrameData& FishEngine::get_current_frame()
{
    return m_Frames[m_FrameNumber % kFrameOverlap];
}

FrameData& FishEngine::get_last_frame()
{
    return m_Frames[(m_FrameNumber - 1) % kFrameOverlap];
}

AllocatedBuffer13 FishEngine::create_buffer13(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer info
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer13 newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

void FishEngine::destroy_buffer(const AllocatedBuffer13& buffer)
{
    vmaDestroyBuffer(m_Allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage FishEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(m_Allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(m_Device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage FishEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer13 uploadbuffer = create_buffer13(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate_submit13([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        if (mipmapped) {
            vkutil::generate_mipmaps(cmd, new_image.image, VkExtent2D{ new_image.imageExtent.width,new_image.imageExtent.height });
        }
        else {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });
    destroy_buffer(uploadbuffer);
    return new_image;
}

void FishEngine::destroy_image(const AllocatedImage& img)
{
    vkDestroyImageView(m_Device, img.imageView, nullptr);
    vmaDestroyImage(m_Allocator, img.image, img.allocation);
}

size_t FishEngine::pad_uniform_buffer_size(size_t originalSize)
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = m_GPUProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

void FishEngine::update()
{
    currentScene.camera.update();
    stats.camera_position = currentScene.camera.m_Position;
    stats.camera_pitch = currentScene.camera.m_Pitch;
    stats.camera_yaw = currentScene.camera.m_Yaw;

    update_scene();
}

void FishEngine::update_scene()
{
    glm::mat4 view = currentScene.camera.get_view_matrix();
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)m_WindowExtents.width / (float)m_WindowExtents.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;
    currentScene.sceneData.view = view;
    currentScene.sceneData.proj = projection;
    currentScene.sceneData.viewproj = projection * view;

    // Draw all objects within the loadedScenes container.
    for (const auto& instance : currentScene.loadedScenes) {

        currentScene.loadedScenes[instance.first]->Update();

        //currentScene.loadedScenes[instance.first]->Draw(glm::mat4{ 1.0f }, mainDrawContext);
    }
}

void FishEngine::immediate_submit13(std::function<void(VkCommandBuffer cmd)>&& function)
{
    const unsigned int timeout = 9999999999;
    VK_CHECK(vkResetFences(m_Device, 1, &m_ImmediateFence));
    VK_CHECK(vkResetCommandBuffer(m_ImmediateCommandBuffer, 0));

    VkCommandBuffer cmd = m_ImmediateCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info2(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, m_ImmediateFence));

    VK_CHECK(vkWaitForFences(m_Device, 1, &m_ImmediateFence, true, timeout));
}

bool FishEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
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

//void FishVulkanEngine::update(float deltatime)
//{
//    //m_PhysicsSystem->update(deltatime);
//}

void GLTFMetallic_Roughness::build_pipelines(FishEngine* engine)
{
    VkShaderModule meshFragShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/mesh.frag.spv", engine->GetDevice(), &meshFragShader)) {
        fmt::println("Error when building the triangle fragment shader module");
    }

    VkShaderModule meshVertexShader;
    if (!Fish::Pipeline::load_shader_module("../../shaders/mesh.vert.spv", engine->GetDevice(), &meshVertexShader)) {
        fmt::println("Error when building the triangle vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialLayout = layoutBuilder.build(engine->GetDevice(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = { engine->GetGPUSceneDataDescriptorLayout(), materialLayout};

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->GetDevice(), &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    Fish::Pipeline::Builder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(engine->GetDrawImage().imageFormat);
    pipelineBuilder.set_depth_format(engine->GetDepthImage().imageFormat);

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = newLayout;

    // finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->GetDevice());

    // create the transparent variant
    pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->GetDevice());

    vkDestroyShaderModule(engine->GetDevice(), meshFragShader, nullptr);
    vkDestroyShaderModule(engine->GetDevice(), meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent) {
        matData.pipeline = &transparentPipeline;
    }
    else {
        matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);


    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.update_set(device, matData.materialSet);

    return matData;
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransformMatrix;

    for (auto& s : mesh->surfaces) {
        Fish::Resource::RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.material = &s.material->data;
        def.bounds = s.bounds;
        def.transformationMatrix = nodeMatrix;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        if (s.material->data.passType == MaterialPass::Transparent) {
            ctx.TransparentSurfaces.push_back(def);
        }
        else {
            ctx.OpaqueSurfaces.push_back(def);
        }
    }

    // recurse down
    Node::Draw(topMatrix, ctx);
}
