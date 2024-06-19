//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_images.h>
#include <vk_initializers.h>

#include <texture.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <VkBootstrap.h> // bootstrap library (if you want to set up without this, see https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code)

#include <glm/gtx/transform.hpp>

#include <chrono>
#include <thread>
#include <fstream>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>
#include <iostream>

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

    //init_imgui(); // disabled until chapter 5

    init_swapchain();

    init_commands();

    init_main_renderpass();

    init_framebuffers();

    init_synchronisation_structures();

    init_descriptors();

    init_pipelines();

    load_images();

    load_meshes();

    init_scene();

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
        .set_minimum_version(1, 1)
        //.set_desired_version(1, 3)
        //.set_required_features_13(features)
        //.set_required_features_12(features12)
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
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);

    m_DeletionQueue.push_function([&]() { vmaDestroyAllocator(m_Allocator); });

    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_GPUProperties);

    std::cout << "The GPU has a minimum buffer alignment of " << m_GPUProperties.limits.minUniformBufferOffsetAlignment << std::endl;
}

void VulkanEngine::init_imgui()
{
    ////1: create descriptor pool for IMGUI
    //// the size of the pool is very oversize, but it's copied from imgui demo itself.
    //VkDescriptorPoolSize pool_sizes[] =
    //{
    //    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    //    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    //    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    //    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    //    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    //    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    //    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    //    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    //    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    //    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    //    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    //};

    //VkDescriptorPoolCreateInfo pool_info = {};
    //pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    //pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    //pool_info.maxSets = 1000;
    //pool_info.poolSizeCount = std::size(pool_sizes);
    //pool_info.pPoolSizes = pool_sizes;

    //VkDescriptorPool imguiPool;
    //VK_CHECK(vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &imguiPool));

    //// 2: initialize imgui library

    ////this initializes the core structures of imgui
    //ImGui::CreateContext();

    ////this initializes imgui for SDL
    //ImGui_ImplSDL2_InitForVulkan(m_pWindow);

    ////this initializes imgui for Vulkan
    //ImGui_ImplVulkan_InitInfo init_info = {};
    //init_info.Instance = m_VkInstance;
    //init_info.PhysicalDevice = m_PhysicalDevice;
    //init_info.Device = m_Device;
    //init_info.Queue = m_GraphicsQueue;
    //init_info.DescriptorPool = imguiPool;
    //init_info.MinImageCount = 3;
    //init_info.ImageCount = 3;
    //init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    //ImGui_ImplVulkan_Init(&init_info, m_MainRenderPass);

    ////execute a gpu command to upload imgui font textures
    //immediate_submit([&](VkCommandBuffer cmd) {
    //    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    //});

    ////clear font textures from cpu data
    //ImGui_ImplVulkan_DestroyFontUploadObjects();

    ////add the destroy the imgui created structures
    //m_DeletionQueue.push_function([=]() {
    //    vkDestroyDescriptorPool(m_Device, imguiPool, nullptr);
    //    ImGui_ImplVulkan_Shutdown();
    //});
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

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].m_CommandPool));

        //allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].m_CommandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_Frames[i].m_CommandBuffer));
                
        m_DeletionQueue.push_function([=]() { vkDestroyCommandPool(m_Device, m_Frames[i].m_CommandPool, nullptr); });
    }    

    //create pool for upload context
    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(m_GraphicsQueueFamily);
    VK_CHECK(vkCreateCommandPool(m_Device, &uploadCommandPoolInfo, nullptr, &m_UploadContext.commandPool));

    m_DeletionQueue.push_function([=]() {
        vkDestroyCommandPool(m_Device, m_UploadContext.commandPool, nullptr);
    });

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_UploadContext.commandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_UploadContext.commandBuffer));
}

void VulkanEngine::init_main_renderpass()
{
    // Colour attachment.

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

    VkSubpassDependency color_dependency = {};
    color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    color_dependency.dstSubpass = 0;
    color_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    color_dependency.srcAccessMask = 0;
    color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Depth attachment.

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.flags = 0;
    depth_attachment.format = m_DepthFormat;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDependency depth_dependency = {};
    depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depth_dependency.dstSubpass = 0;
    depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.srcAccessMask = 0;
    depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Render pass and subpass info.

    VkSubpassDescription subpass = {}; // 1 subpass is the minimum
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;    
    subpass.pDepthStencilAttachment = &depth_attachment_ref; //hook the depth attachment into the subpass

    //array of attachments and dependencies, for colour and depth.
    const int attachmentSize = 2, dependencySize = 2;    
    VkAttachmentDescription attachments[attachmentSize] = { color_attachment, depth_attachment };
    VkSubpassDependency dependencies[dependencySize] = { color_dependency, depth_dependency };

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachmentSize;
    render_pass_info.pAttachments = &attachments[0];
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = dependencySize;
    render_pass_info.pDependencies = &dependencies[0];

    // Create and clean.

    VK_CHECK(vkCreateRenderPass(m_Device, &render_pass_info, nullptr, &m_MainRenderPass));

    m_DeletionQueue.push_function([=]() { vkDestroyRenderPass(m_Device, m_MainRenderPass, nullptr); });
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

        const int attachmentSize = 2;
        VkImageView attachments[attachmentSize];
        attachments[0] = m_SwapchainImageViews[i];
        attachments[1] = m_DepthImageView;

        fb_info.pAttachments = attachments;
        fb_info.attachmentCount = attachmentSize;

        VK_CHECK(vkCreateFramebuffer(m_Device, &fb_info, nullptr, &m_FrameBuffers[i]));

        m_DeletionQueue.push_function([=]() {
            vkDestroyFramebuffer(m_Device, m_FrameBuffers[i], nullptr);
            vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
        });
    }
}

void VulkanEngine::init_descriptors()
{
    //create a descriptor pool that will hold 10 uniform buffers and 10 dynamic uniform buffers and 10 storage buffers and 10 combined image samplers.
    std::vector<VkDescriptorPoolSize> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &m_DescriptorPool);

    VkDescriptorSetLayoutBinding cameraBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VkDescriptorSetLayoutBinding sceneBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

    //const int bindingCount = 2;
    VkDescriptorSetLayoutBinding bindings[] = { cameraBinding, sceneBinding };

    // Global Set Layout.
    VkDescriptorSetLayoutCreateInfo setinfo = {};
    setinfo.bindingCount = 2;
    setinfo.flags = 0;
    setinfo.pNext = nullptr;
    setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setinfo.pBindings = bindings;

    vkCreateDescriptorSetLayout(m_Device, &setinfo, nullptr, &m_GlobalSetLayout);

    // Object bind.
    VkDescriptorSetLayoutBinding objectBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VkDescriptorSetLayoutCreateInfo set2info = {};
    set2info.bindingCount = 1;
    set2info.flags = 0;
    set2info.pNext = nullptr;
    set2info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set2info.pBindings = &objectBinding;

    vkCreateDescriptorSetLayout(m_Device, &set2info, nullptr, &m_ObjectSetLayout);

    // Texture bind.
    VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    VkDescriptorSetLayoutCreateInfo set3info = {};
    set3info.bindingCount = 1;
    set3info.flags = 0;
    set3info.pNext = nullptr;
    set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set3info.pBindings = &textureBind;

    vkCreateDescriptorSetLayout(m_Device, &set3info, nullptr, &m_SingleTextureSetLayout);

    // Scene param buffer.
    const size_t sceneParamBufferSize = kFrameOverlap * pad_uniform_buffer_size(sizeof(GPUSceneData));
    m_SceneParametersBuffer = create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        // Camera buffer.
        m_Frames[i].cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Object buffer.
        const int kMaxObjects = 10000;
        m_Frames[i].objectBuffer = create_buffer(sizeof(GPUObjectData) * kMaxObjects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Global descriptor.
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_GlobalSetLayout;

        vkAllocateDescriptorSets(m_Device, &allocInfo, &m_Frames[i].globalDescriptor);

        // Object descriptor.
        VkDescriptorSetAllocateInfo objectSetAlloc = {};
        objectSetAlloc.pNext = nullptr;
        objectSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        objectSetAlloc.descriptorPool = m_DescriptorPool;
        objectSetAlloc.descriptorSetCount = 1;
        objectSetAlloc.pSetLayouts = &m_ObjectSetLayout;

        vkAllocateDescriptorSets(m_Device, &objectSetAlloc, &m_Frames[i].objectDescriptor);

        // Camera info.
        VkDescriptorBufferInfo cameraInfo;
        cameraInfo.buffer = m_Frames[i].cameraBuffer.buffer;
        cameraInfo.offset = 0;
        cameraInfo.range = sizeof(GPUCameraData);

        // Scene info.
        VkDescriptorBufferInfo sceneInfo;
        sceneInfo.buffer = m_SceneParametersBuffer.buffer;
        sceneInfo.offset = 0;
        sceneInfo.range = sizeof(GPUSceneData);

        // Object info.
        VkDescriptorBufferInfo objectInfo;
        objectInfo.buffer = m_Frames[i].objectBuffer.buffer;
        objectInfo.offset = 0;
        objectInfo.range = sizeof(GPUObjectData) * kMaxObjects;

        VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_Frames[i].globalDescriptor, &cameraInfo, 0);
        VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_Frames[i].globalDescriptor, &sceneInfo, 1);
        VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_Frames[i].objectDescriptor, &objectInfo, 0);

        //const int writeCount = 3;
        VkWriteDescriptorSet setWrites[] = { cameraWrite, sceneWrite, objectWrite };

        vkUpdateDescriptorSets(m_Device, 3, setWrites, 0, nullptr);
    }

    ////information about the binding.
    //VkDescriptorSetLayoutBinding camBufferBinding = {};
    //camBufferBinding.binding = 0;
    //camBufferBinding.descriptorCount = 1;
    //camBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;   
    //camBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    //clean 
    m_DeletionQueue.push_function([&]() {

        vmaDestroyBuffer(m_Allocator, m_SceneParametersBuffer.buffer, m_SceneParametersBuffer.allocation);

        vkDestroyDescriptorSetLayout(m_Device, m_ObjectSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_Device, m_GlobalSetLayout, nullptr);
        //vkDestroyDescriptorSetLayout(m_Device, _singleTextureSetLayout, nullptr);

        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

        for (int i = 0; i < kFrameOverlap; ++i)
        {
            vmaDestroyBuffer(m_Allocator, m_Frames[i].cameraBuffer.buffer, m_Frames[i].cameraBuffer.allocation);

            vmaDestroyBuffer(m_Allocator, m_Frames[i].objectBuffer.buffer, m_Frames[i].objectBuffer.allocation);
        }
    });
}

void VulkanEngine::init_pipelines()
{
    // TODO: Load shaders in one function, and shorten the function into a "check_load" type function.

    VkShaderModule colorMeshShader;
    if (!load_shader_module("../../shaders/default_lit.frag.spv", &colorMeshShader))
    {
        fmt::println("Error when building the defaultLitShader shader module");
    }
    else {
        fmt::println("defaultLitShader shader successfully loaded");
    }

    VkShaderModule texturedMeshShader;
    if (!load_shader_module("../../shaders/textured_lit.frag.spv", &texturedMeshShader))
    {
        fmt::println("Error when building the texturedLitShader shader module");
    }
    else {
        fmt::println("texturedLitShader shader successfully loaded");
    }

    VkShaderModule meshVertShader;
    if (!load_shader_module("../../shaders/triangleMesh.vert.spv", &meshVertShader))
    {
        fmt::println("Error when building the triangleMeshShader shader module");
    }
    else {
        fmt::println("triangleMeshShader shader successfully loaded");
    }
      

    //build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;

    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colorMeshShader));

    //we start from just the default empty pipeline layout info
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

    //setup push constants
    VkPushConstantRange push_constant;
    //offset 0
    push_constant.offset = 0;
    //size of a MeshPushConstant struct
    push_constant.size = sizeof(MeshPushConstants);
    //for the vertex shader
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
    mesh_pipeline_layout_info.pushConstantRangeCount = 1;

    VkDescriptorSetLayout setLayouts[] = { m_GlobalSetLayout, m_ObjectSetLayout };

    mesh_pipeline_layout_info.setLayoutCount = 2;
    mesh_pipeline_layout_info.pSetLayouts = setLayouts;

    VkPipelineLayout meshPipLayout;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &mesh_pipeline_layout_info, nullptr, &meshPipLayout));

    //we start from  the normal mesh layout
    VkPipelineLayoutCreateInfo textured_pipeline_layout_info = mesh_pipeline_layout_info;

    VkDescriptorSetLayout texturedSetLayouts[] = { m_GlobalSetLayout, m_ObjectSetLayout, m_SingleTextureSetLayout };

    textured_pipeline_layout_info.setLayoutCount = 3;
    textured_pipeline_layout_info.pSetLayouts = texturedSetLayouts;

    VkPipelineLayout texturedPipeLayout;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &textured_pipeline_layout_info, nullptr, &texturedPipeLayout));

    //hook the push constants layout
    pipelineBuilder.pipelineLayout = meshPipLayout;

    //vertex input controls how to read vertices from vertex buffers. We arent using it yet
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

    //we dont use multisampling, so just run the default one
    pipelineBuilder.multisampler = vkinit::multisampling_state_create_info();

    //a single blend attachment with no blending and writing to RGBA
    pipelineBuilder.colourBlendAttachment = vkinit::color_blend_attachment_state();

    //default depthtesting
    pipelineBuilder.depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    //build the mesh pipeline

    Fish::VertexInputDescription vertexDescription = Fish::Vertex::get_vertex_description();

    //connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder.vertexInputState.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.vertexInputState.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

    pipelineBuilder.vertexInputState.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.vertexInputState.vertexBindingDescriptionCount = vertexDescription.bindings.size();

    //build the mesh triangle pipeline
    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);

    create_material(meshPipeline, meshPipLayout, "defaultmesh");

    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, texturedMeshShader));

    pipelineBuilder.pipelineLayout = texturedPipeLayout;
    VkPipeline texPipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);
    create_material(texPipeline, texturedPipeLayout, "texturedmesh");

    vkDestroyShaderModule(m_Device, meshVertShader, nullptr);
    vkDestroyShaderModule(m_Device, colorMeshShader, nullptr);
    vkDestroyShaderModule(m_Device, texturedMeshShader, nullptr);

    m_DeletionQueue.push_function([=]() {
        vkDestroyPipeline(m_Device, meshPipeline, nullptr);
        vkDestroyPipeline(m_Device, texPipeline, nullptr);

        vkDestroyPipelineLayout(m_Device, meshPipLayout, nullptr);
        vkDestroyPipelineLayout(m_Device, texturedPipeLayout, nullptr);
    });
}

void VulkanEngine::init_synchronisation_structures()
{
    //create synchronization structures

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)

    //for the semaphores we don't need any flags
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

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
}

void VulkanEngine::init_scene()
{
    RenderObject monkey;
    monkey.pMesh = get_mesh("monkey");
    monkey.pMaterial = get_material("defaultmesh");
    monkey.transformMatrix = glm::mat4{ 1.0f };

    m_RenderObjects.push_back(monkey);

    // We create 1 monkey, add it as the first thing to the renderables array, 
    // and then we create a lot of triangles in a grid, and put them around the monkey.

    for (int x = -20; x <= 20; x++) {
        for (int y = -20; y <= 20; y++) {

            RenderObject triangle;
            triangle.pMesh = get_mesh("triangle");
            triangle.pMaterial = get_material("defaultmesh");
            glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
            glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
            triangle.transformMatrix = translation * scale;

            m_RenderObjects.push_back(triangle);
        }
    }

    RenderObject map;
    map.pMesh = get_mesh("empire");
    map.pMaterial = get_material("texturedmesh");
    map.transformMatrix = glm::translate(glm::vec3{ 5,-10,0 });
    m_RenderObjects.push_back(map);

    //create a sampler for the texture
    VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

    VkSampler blockySampler;
    vkCreateSampler(m_Device, &samplerInfo, nullptr, &blockySampler);

    Material* texturedMat = get_material("texturedmesh");

    //allocate the descriptor set for single-texture to use on the material
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_SingleTextureSetLayout;

    vkAllocateDescriptorSets(m_Device, &allocInfo, &texturedMat->textureSet);

    //write to the descriptor set so that it points to our empire_diffuse texture
    VkDescriptorImageInfo imageBufferInfo;
    imageBufferInfo.sampler = blockySampler;
    imageBufferInfo.imageView = m_Textures["empire_diffuse"].imageView;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);

    vkUpdateDescriptorSets(m_Device, 1, &texture1, 0, nullptr);
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

    //depth image size will match the window
    VkExtent3D depthImageExtent = {
        m_WindowExtents.width,
        m_WindowExtents.height,
        1
    };

    //hardcoding the depth format to 32 bit float
    m_DepthFormat = VK_FORMAT_D32_SFLOAT;

    //the depth image will be an image with the format we selected and Depth Attachment usage flag
    VkImageCreateInfo dimg_info = vkinit::image_create_info(m_DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent); // this flag allows depth image to be used for z-testing.

    //for the depth image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(m_Allocator, &dimg_info, &dimg_allocinfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);

    //build an image-view for the depth image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(m_DepthFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(m_Device, &dview_info, nullptr, &m_DepthImageView));

    m_DeletionQueue.push_function([=]() { 
        destroy_swapchain();

        // depth
        vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
    });
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
        //--m_FrameNumber;
        vkWaitForFences(m_Device, 1, &get_current_frame().m_RenderFence, true, timeout);
        //++m_FrameNumber;

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

void VulkanEngine::render()
{
    const unsigned int timeout = 1000000000;

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(m_Device, 1, &get_current_frame().m_RenderFence, true, timeout));
    VK_CHECK(vkResetFences(m_Device, 1, &get_current_frame().m_RenderFence));

    //request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, timeout, get_current_frame().m_PresentSemaphore, nullptr, &swapchainImageIndex));


    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(get_current_frame().m_CommandBuffer, 0));

    // get local frame command buffer
    VkCommandBuffer cmd = get_current_frame().m_CommandBuffer;

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

    // clear at a depth of 1
    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.0f;

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
    VkClearValue clearValues[] = { clearValue, depthClear };
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Actually send out rendering commands:
    // 
    // 
    
    if (m_SelectedShader == SelectedShader::ColouredPipeline)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ColouredTrianglePipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }
    else if (m_SelectedShader == SelectedShader::RedPipeline)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RedTrianglePipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }
    else if (m_SelectedShader == SelectedShader::MeshPipeline)
    {
        render_objects(cmd, m_RenderObjects.data(), m_RenderObjects.size());
    } 

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
    submit.pWaitSemaphores = &get_current_frame().m_PresentSemaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &get_current_frame().m_RenderSemaphore;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submit, get_current_frame().m_RenderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame().m_RenderSemaphore;
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

                    // Increment shader 
                    m_SelectedShader = (SelectedShader)((int)m_SelectedShader + 1);

                    // Don't exceed max shader
                    if (m_SelectedShader == SelectedShader::MaxPipelines) {

                        // Reset shader 
                        m_SelectedShader = (SelectedShader)(0);
                    }
                    fmt::println("Toggle next shader: ", (int)m_SelectedShader);
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

        render();
    }
}

void VulkanEngine::load_meshes()
{
    //make the array 3 vertices long
    m_TriangleMesh.vertices.resize(3);

    //vertex positions
    m_TriangleMesh.vertices[0].position = { 1.f, 1.f, 0.0f };
    m_TriangleMesh.vertices[1].position = { -1.f, 1.f, 0.0f };
    m_TriangleMesh.vertices[2].position = { 0.f,-1.f, 0.0f };

    //vertex colors, all green
    m_TriangleMesh.vertices[0].colour = { 0.f, 1.f, 0.0f }; //pure green
    m_TriangleMesh.vertices[1].colour = { 0.f, 1.f, 0.0f }; //pure green
    m_TriangleMesh.vertices[2].colour = { 0.f, 1.f, 0.0f }; //pure green

    //we don't care about the vertex normals

    // load mesh
    m_MonkeyMesh.load_from_obj("../../assets/monkey_smooth.obj");
    m_LostEmpireMesh.load_from_obj("../../assets/lost_empire.obj");

    upload_mesh(m_TriangleMesh);
    upload_mesh(m_MonkeyMesh);
    upload_mesh(m_LostEmpireMesh);

    m_Meshes["monkey"] = m_MonkeyMesh;
    m_Meshes["triangle"] = m_TriangleMesh;
    m_Meshes["empire"] = m_LostEmpireMesh;
}

void VulkanEngine::load_images()
{
    Texture lostEmpire;

    Fish::Textures::load_image_from_file(*this, "../../assets/lost_empire-RGBA.png", lostEmpire.image);

    VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
    vkCreateImageView(m_Device, &imageinfo, nullptr, &lostEmpire.imageView);

    m_Textures["empire_diffuse"] = lostEmpire;
}

void VulkanEngine::upload_mesh(Fish::Mesh& mesh)
{
    const size_t bufferSize = mesh.vertices.size() * sizeof(Fish::Vertex);
    //allocate staging buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.pNext = nullptr;

    stagingBufferInfo.size = bufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    //let the VMA library know that this data should be on CPU RAM
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    AllocatedBuffer stagingBuffer;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_Allocator, &stagingBufferInfo, &vmaallocInfo, &stagingBuffer.buffer, &stagingBuffer.allocation, nullptr));

    //

    //copy vertex data
    void* data;
    vmaMapMemory(m_Allocator, stagingBuffer.allocation, &data);

    memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Fish::Vertex));

    vmaUnmapMemory(m_Allocator, stagingBuffer.allocation);

    //

    //allocate vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    vertexBufferInfo.size = bufferSize;
    //this buffer is going to be used as a Vertex Buffer
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    //let the VMA library know that this data should be GPU native
    vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_Allocator, &vertexBufferInfo, &vmaallocInfo, &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    //

    immediate_submit([=](VkCommandBuffer cmd) {
        VkBufferCopy copy;
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
    });

    //

    //add the destruction of mesh buffer to the deletion queue
    m_DeletionQueue.push_function([=]() { vmaDestroyBuffer(m_Allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation); });
    vmaDestroyBuffer(m_Allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
    Material mat;
    mat.pipeline = pipeline;
    mat.pipelineLayout = layout;
    m_Materials[name] = mat;
    return &m_Materials[name];
}

Material* VulkanEngine::get_material(const std::string& name)
{
    //search for the object, and return nullptr if not found
    auto it = m_Materials.find(name);
    if (it == m_Materials.end()) {
        return nullptr;
    }
    else {
        return &(*it).second;
    }
}

Fish::Mesh* VulkanEngine::get_mesh(const std::string& name)
{
    auto it = m_Meshes.find(name);
    if (it == m_Meshes.end()) {
        return nullptr;
    }
    else {
        return &(*it).second;
    }
}

void VulkanEngine::render_objects(VkCommandBuffer cmd, RenderObject* first, int count)
{
    //make a model view matrix for rendering the object
    //camera view
    glm::vec3 camPos = { 0.f, -6.f, -10.f };

    glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);

    //camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f); // todo remove magic numbers
    projection[1][1] *= -1;

    //fill a GPU camera data struct
    GPUCameraData camData;
    camData.projectionMatrix = projection;
    camData.viewMatrix = view;
    camData.viewProjectionMatrix = projection * view; // matrix multiplication is reversed.

    //and copy it to the buffer
    void* data;
    vmaMapMemory(m_Allocator, get_current_frame().cameraBuffer.allocation, &data);
    memcpy(data, &camData, sizeof(GPUCameraData));
    vmaUnmapMemory(m_Allocator, get_current_frame().cameraBuffer.allocation);

    // write into the scene buffer
    float framed = (m_FrameNumber / 120.0f);
    m_SceneParameters.ambientColour = { sin(framed), 0, cos(framed), 1 };
    char* sceneData;
    vmaMapMemory(m_Allocator, m_SceneParametersBuffer.allocation, (void**)&sceneData);
    int frameIndex = m_FrameNumber & kFrameOverlap;
    sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
    memcpy(sceneData, &m_SceneParameters, sizeof(GPUSceneData));
    vmaUnmapMemory(m_Allocator, m_SceneParametersBuffer.allocation);    

    // write into the object buffer (shader storage buffer)
    void* objectData;
    vmaMapMemory(m_Allocator, get_current_frame().objectBuffer.allocation, &objectData);
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    for (int i = 0; i < count; i++)
    {
        RenderObject& object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }
    vmaUnmapMemory(m_Allocator, get_current_frame().objectBuffer.allocation);

    Fish::Mesh* lastMesh = nullptr;
    Material* lastMaterial = nullptr;
    for (int i = 0; i < count; i++)
    {
        RenderObject& object = first[i];

        //only bind the pipeline if it doesn't match with the already bound one
        if (object.pMaterial != lastMaterial) {

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipeline);
            lastMaterial = object.pMaterial;

            //offset for our scene buffer
            uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipelineLayout, 0, 1, &get_current_frame().globalDescriptor, 1, &uniform_offset);

            //object data descriptor
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipelineLayout, 1, 1, &get_current_frame().objectDescriptor, 0, nullptr);

            //texture descriptor
            if (object.pMaterial->textureSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipelineLayout, 2, 1, &object.pMaterial->textureSet, 0, nullptr);
            }






        }

        glm::mat4 model = object.transformMatrix;

        //final render matrix, that we are calculating on the cpu
        //glm::mat4 mesh_matrix = projection * view * model;

        MeshPushConstants constants;
        constants.matrix = object.transformMatrix;

        //upload the mesh to the GPU via push constants
        vkCmdPushConstants(cmd, object.pMaterial->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

        //only bind the mesh if it's a different one from last bind
        if (object.pMesh != lastMesh) {
            //bind the mesh vertex buffer with offset 0
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &object.pMesh->vertexBuffer.buffer, &offset);
            lastMesh = object.pMesh;
        }
        //we can now draw
        vkCmdDraw(cmd, object.pMesh->vertices.size(), 1, 0, i);
    }
}

FrameData& VulkanEngine::get_current_frame()
{
    return m_Frames[m_FrameNumber % kFrameOverlap];
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;

    AllocatedBuffer newBuffer;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, nullptr));

    return newBuffer;
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = m_GPUProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
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
    pipelineInfo.pDepthStencilState = &depthStencil;
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
