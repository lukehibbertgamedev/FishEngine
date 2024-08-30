﻿//> includes
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

// We set a *global* pointer for the vulkan engine singleton reference. 
// We do that instead of a typical singleton because we want to control explicitly when is the class initalized and destroyed. 
// The normal Cpp singleton pattern doesnt give us control over that.
FishVulkanEngine* loadedEngine = nullptr;

FishVulkanEngine& FishVulkanEngine::Get() { return *loadedEngine; }

void FishVulkanEngine::init()
{
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
    initialise_imgui(); // Required to be called after Vulkan initialisation.

    // initialise entity component systems
    //init_ecs();
    
    // Load meshes.
    testMeshes = loadGltfMeshes(loadedEngine, "..//..//assets//basicmesh.glb").value();

    Fish::Timer::EngineTimer _timer;
    m_EngineTimer = _timer;

    // everything went fine
    m_IsInitialized = true;
}

void FishVulkanEngine::initialise_vulkan()
{
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

    //use vkbootstrap to select a gpu. 
    //We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_instance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        //.set_desired_version(1, 3)
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

void FishVulkanEngine::initialise_imgui()
{
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
    VkCommandBuffer cmd = get_current_frame().m_CommandBuffer;
    immediate_submit13([=](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    // Ensure to delete ImGui related memory.
    m_DeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(m_Device, imguiPool, nullptr);
    });
}

void FishVulkanEngine::initialise_swapchain()
{
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

void FishVulkanEngine::initialise_commands()
{
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].m_CommandPool));

        //allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].m_CommandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_Frames[i].m_CommandBuffer));
                
        m_DeletionQueue.push_function([=]() { vkDestroyCommandPool(m_Device, m_Frames[i].m_CommandPool, nullptr); });
    }    

    // > Immediate Commands
    VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_ImmediateCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo immediateCmdAllocInfo = vkinit::command_buffer_allocate_info(m_ImmediateCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_Device, &immediateCmdAllocInfo, &m_ImmediateCommandBuffer));

    m_DeletionQueue.push_function([=]() { vkDestroyCommandPool(m_Device, m_ImmediateCommandPool, nullptr); });
    // < Immediate Commands

    //create pool for upload context
    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(m_GraphicsQueueFamily);
    VK_CHECK(vkCreateCommandPool(m_Device, &uploadCommandPoolInfo, nullptr, &m_UploadContext.commandPool));

    m_DeletionQueue.push_function([=]() {
        vkDestroyCommandPool(m_Device, m_UploadContext.commandPool, nullptr);
    });

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo uploadCmdAllocInfo = vkinit::command_buffer_allocate_info(m_UploadContext.commandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_Device, &uploadCmdAllocInfo, &m_UploadContext.commandBuffer));
}

void FishVulkanEngine::initialise_pipelines()
{
    // Compute Pipelines.
    init_background_pipelines();

    // Graphics Pipelines.
    //init_triangle_pipeline();
    init_mesh_pipeline();
}

void FishVulkanEngine::init_background_pipelines()
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
    if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", m_Device, &gradientColourShader))
    {
        fmt::print("Error when building the gradient colour shader \n");
    }

    VkShaderModule gradientShader;
    if (!vkutil::load_shader_module("../../shaders/gradient.comp.spv", m_Device, &gradientShader))
    {
        fmt::print("Error when building the compute/gradient shader \n");
    }
    
    VkShaderModule skyShader;
    if (!vkutil::load_shader_module("../../shaders/sky.comp.spv", m_Device, &skyShader))
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

void FishVulkanEngine::init_mesh_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle.frag.spv", m_Device, &triangleFragShader)) {
        fmt::print("Error when building the triangle fragment shader module");
    }
    else {
        fmt::print("Coloured triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle_mesh.vert.spv", m_Device, &triangleVertexShader)) {
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

    VK_CHECK(vkCreatePipelineLayout(m_Device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

    Fish::PipelineBuilder13 pipelineBuilder;

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
    //pipelineBuilder.disable_blending();
    //additive blending
    pipelineBuilder.enable_blending_additive();

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

void FishVulkanEngine::initialise_synchronisation_structures()
{
    //create synchronization structures

    //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    //for the semaphores we don't need any flags
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_Frames[i].m_RenderFence));
        
        VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();
        VK_CHECK(vkCreateFence(m_Device, &uploadFenceCreateInfo, nullptr, &m_UploadContext.uploadFence));

        // queue destruction of fences.
        m_DeletionQueue.push_function([=]() { 
            vkDestroyFence(m_Device, m_Frames[i].m_RenderFence, nullptr); 
            vkDestroyFence(m_Device, m_UploadContext.uploadFence, nullptr);
        });

        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].m_PresentSemaphore));
        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].m_RenderSemaphore));

        // queue destruction of semaphores.
        m_DeletionQueue.push_function([=]() {
            vkDestroySemaphore(m_Device, m_Frames[i].m_PresentSemaphore, nullptr);
            vkDestroySemaphore(m_Device, m_Frames[i].m_RenderSemaphore, nullptr);
        });
    }    

    // > Immediate Fence
    VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_ImmediateFence));
    m_DeletionQueue.push_function([=]() { vkDestroyFence(m_Device, m_ImmediateFence, nullptr); });
    // < Immediate Fence
}

void FishVulkanEngine::create_swapchain(uint32_t width, uint32_t height)
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

void FishVulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < m_SwapchainImageViews.size(); i++) {

        vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
    }
}

void FishVulkanEngine::rebuild_swapchain()
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

void FishVulkanEngine::resize_swapchain()
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

void FishVulkanEngine::initialise_descriptors()
{
    // initialize the descriptor allocator with 10 sets, and 
    // 1 descriptor per set of type VK_DESCRIPTOR_TYPE_STORAGE_IMAGE. 
    // Thats the type used for a image that can be written to from a compute shader.
    // use the layout builder to build the descriptor set layout we need, which is 
    // a layout with only 1 single binding at binding number 0, of type 
    // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE too (matching the pool).

    //create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    m_GlobalDescriptorAllocator.init_pool(m_Device, 10, sizes);

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        m_DrawImageDescriptorLayout = builder.build(m_Device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    //allocate a descriptor set for our draw image
    m_DrawImageDescriptors = m_GlobalDescriptorAllocator.allocate(m_Device, m_DrawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = m_DrawImage.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = m_DrawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(m_Device, 1, &drawImageWrite, 0, nullptr);

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_DeletionQueue.push_function([&]() {
        m_GlobalDescriptorAllocator.destroy_pool(m_Device);
        vkDestroyDescriptorSetLayout(m_Device, m_DrawImageDescriptorLayout, nullptr);
    });
}

void FishVulkanEngine::cleanup()
{
    // Delete in the opposite order to what they were created, to avoid dependency errors.
    
    const unsigned int timeout = 1000000000;

    if (m_IsInitialized) {

        //make sure the GPU has stopped doing its things
        vkWaitForFences(m_Device, 1, &get_current_frame().m_RenderFence, true, timeout);

        for (auto& mesh : testMeshes) {
            destroy_buffer(mesh->meshBuffers.indexBuffer);
            destroy_buffer(mesh->meshBuffers.vertexBuffer);
        }

        m_DeletionQueue.flush();
        
        //vmaDestroyAllocator(m_Allocator);

        vkDestroyDevice(m_Device, nullptr);
        
        vkDestroySurfaceKHR(m_VkInstance, m_SurfaceKHR, nullptr);

        vkb::destroy_debug_utils_messenger(m_VkInstance, m_DebugMessenger);

        vkDestroyInstance(m_VkInstance, nullptr);

        SDL_DestroyWindow(m_pWindow);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void FishVulkanEngine::prepare_imgui()
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

void FishVulkanEngine::create_imgui_draw_data()
{
    // Begin Debug Overlay
    {
        ImGui::Begin("Debug Overlay", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar);
        imgui_debug_data();
        ImGui::End(); 
    }
    // End Debug Overlay

    // Begin Background Effects
    {
        if (ImGui::Begin("Background")) {

            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected effect: ", selected.name);

            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();
    }
    // End Background Effects

    // Begin render scale slider
    {
        if (ImGui::Begin("Zoom")) {
            ImGui::SliderFloat("Zoom factor", &renderScale, 0.3f, 1.f);
        }
        ImGui::End();
    }
    // End render scale slider
}

void FishVulkanEngine::imgui_debug_data()
{

    ImGui::Text("Debug data for Fish engine.");

    ImGui::Text("Elapsed: %f", m_EngineTimer.engine_time());
    ImGui::Text("Delta Time: %f", m_EngineTimer.delta_time());
    ImGui::Text("Camera Position: %f, %f, %f", m_Camera.m_Position.x, m_Camera.m_Position.y, m_Camera.m_Position.z);
    ImGui::Text("Camera Pitch/Yaw: %f/%f", m_Camera.m_Pitch, m_Camera.m_Yaw);
}

//void FishVulkanEngine::imgui_object_hierarchy()
//{
//    int i = 0;
//    for (auto& obj : Fish::ResourceManager::Get().m_Scene.m_SceneObjects)
//    {
//        std::string name = "Object #" + std::to_string(i);
//        if (ImGui::TreeNode(name.c_str()))
//        {
//            ImGui::NewLine();
//
//            // Local cache, with thanks to MellOH for the idea and functional implementation.
//            float position[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
//            float rotation[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
//            float scale[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
//
//            // Read transform.
//            position[0] = obj.transform.position.x;
//            position[1] = obj.transform.position.y;
//            position[2] = obj.transform.position.z;
//            rotation[0] = obj.transform.eulerRotation.x;
//            rotation[1] = obj.transform.eulerRotation.y;
//            rotation[2] = obj.transform.eulerRotation.z;
//            scale[0] = obj.transform.scale.x;
//            scale[1] = obj.transform.scale.y;
//            scale[2] = obj.transform.scale.z;
//
//            // Modify transform.
//            ImGui::DragFloat3("position", position);
//            ImGui::DragFloat3("rotation", rotation);
//            ImGui::DragFloat3("scale", scale);
//
//            // Write transform.
//            obj.transform.position.x = position[0];
//            obj.transform.position.y = position[1];
//            obj.transform.position.z = position[2];
//            obj.transform.eulerRotation.x = rotation[0];
//            obj.transform.eulerRotation.y = rotation[1];
//            obj.transform.eulerRotation.z = rotation[2];
//            obj.transform.scale.x = scale[0];
//            obj.transform.scale.y = scale[1];
//            obj.transform.scale.z = scale[2];
//
//            // Calculate new model matrix.
//            obj.update_model_matrix();
//
//            ImGui::TreePop(); // Must be after every node.
//        }
//        i++; // Iterate object index for the name.
//    }
//}

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

GPUMeshBuffers FishVulkanEngine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
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

void FishVulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(m_SwapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void FishVulkanEngine::draw()
{
    // Wait for the render fence to enter a signalled state meaning the GPU has finished 
    // rendering the previous frame and the GPU is now synchronised with the CPU.
    const unsigned int timeout = 1000000000;
    VK_CHECK(vkWaitForFences(m_Device, 1, &get_current_frame().m_RenderFence, true, timeout));

    // Clear all per-frame memory allocation.
    get_current_frame().deletionQueue.flush();

    // The swapchain image index from our different images that we want to request for the next frame.
    uint32_t swapchainImageIndex;

    // Request the next swapchain image.
    VkResult e = vkAcquireNextImageKHR(m_Device, m_Swapchain, timeout, get_current_frame().m_PresentSemaphore, nullptr, &swapchainImageIndex);

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
    VK_CHECK(vkResetFences(m_Device, 1, &get_current_frame().m_RenderFence));

    // Since everything is synchronised and the previous frame is not executing any more commands, we can safely
    // reset our command buffer so that we can begin recording commands for this frame.
    VK_CHECK(vkResetCommandBuffer(get_current_frame().m_CommandBuffer, 0));

    // The current frame's command buffer in shorthand for easier writing.
    VkCommandBuffer cmd = get_current_frame().m_CommandBuffer;

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

    // Render our clear colour, or set our background effects.
    draw_background(cmd);

    // Prepare our draw image and a depth image (since geometry can be rendered on top of each other) before calling draw calls.
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // Render our meshes, primitives, and other into the swapchain.
    draw_geometry(cmd);

    // Transition the draw and swapchain image into their optimal transfer layouts.
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
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
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().m_PresentSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().m_RenderSemaphore);
    VkSubmitInfo2 submit = vkinit::submit_info2(&cmdinfo, &signalInfo, &waitInfo);

    // Submit our command buffer to the graphics queue. Our render fence will be signaled once all submitted command buffers have been executed.
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, get_current_frame().m_RenderFence));

    // Prepare presentation of the swapchain that we rendered into to be displayed to our screen.
    // We need to wait on the render semaphore to signal that vkQueueSubmit2 executed properly before we can issue the present request.
    // It is necessary that all drawing commands have finished before the image can be displayed.

    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame().m_RenderSemaphore;
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

void FishVulkanEngine::run()
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
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    m_StopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    m_StopRendering = false;
                }
            }

            // Have the camera process the events.
            m_Camera.processSDLEvent(e);

            // Have ImGui process the events.
            ImGui_ImplSDL2_ProcessEvent(&e);

        } // End event loop.

        // do not draw.
        if (m_StopRendering) {

            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // if window has been resized in any way, the swapchain needs to be resized to render images to our new window.
        if (swapchain_resize_requested) {
            resize_swapchain();
        }

        m_EngineTimer.tick();

        //update(m_EngineTimer.delta_time());

        // Set all draw data for ImGui so that the render loop can submit this data.
        prepare_imgui();        

        // Main draw loop.
        draw();
    }
}

void FishVulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    //begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = m_DrawExtent.width;
    viewport.height = m_DrawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = m_DrawExtent.width;
    scissor.extent.height = m_DrawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    GPUDrawPushConstants push_constants = { .worldMatrix = glm::mat4{1.0f} };

    // > matrix view.
    glm::mat4 view = glm::translate(glm::vec3{ 0,0,-5 });
    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)m_DrawExtent.width / (float)m_DrawExtent.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    push_constants.worldMatrix = projection * view;
    // < matrix view.

    // > draw mesh.
    push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1, testMeshes[2]->surfaces[0].startIndex, 0, 0);
    // < draw mesh.


    vkCmdEndRendering(cmd);
}

void FishVulkanEngine::draw_background(VkCommandBuffer cmd)
{
    // Get the currently selected effect set by the ImGui overlay.
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    // Bind our selected effect's pipeline to the command buffer, configured as a compute pipeline.
    // Compute pipelines handle calculations that can be done in parallel with the graphics pipeline. It also flows data through a programmable compute shader stage.
    // This compute shader stage is not part of the graphics pipeline and they have their own pipeline that runs independently.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    // Bind the 
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DrawImageDescriptors, 0, nullptr);

    // Submit the push constants to the GPU to be accessible within the shader.
    vkCmdPushConstants(cmd, m_GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
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

FrameData& FishVulkanEngine::get_current_frame()
{
    return m_Frames[m_FrameNumber % kFrameOverlap];
}

AllocatedBuffer13 FishVulkanEngine::create_buffer13(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

void FishVulkanEngine::destroy_buffer(const AllocatedBuffer13& buffer)
{
    vmaDestroyBuffer(m_Allocator, buffer.buffer, buffer.allocation);
}

size_t FishVulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = m_GPUProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

void FishVulkanEngine::immediate_submit11(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VkCommandBuffer cmd = m_UploadContext.commandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //execute the function
    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vkinit::submit_info(&cmd);

    //submit command buffer to the queue and execute it.
    // _uploadFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_UploadContext.uploadFence));

    vkWaitForFences(m_Device, 1, &m_UploadContext.uploadFence, true, 9999999999);
    vkResetFences(m_Device, 1, &m_UploadContext.uploadFence);

    // reset the command buffers inside the command pool
    vkResetCommandPool(m_Device, m_UploadContext.commandPool, 0);
}

void FishVulkanEngine::immediate_submit13(std::function<void(VkCommandBuffer cmd)>&& function)
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

bool FishVulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
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
