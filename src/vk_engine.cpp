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

    //init_descriptors11(); // 1.1 Implementation
    init_descriptors13();

    //init_pipelines11(); // 1.1 Implementation
    init_pipelines13();

    // load textures -> meshes -> scene
    //Fish::ResourceManager::Get().init();

    // Required to be called after Vulkan initialisation.
    //init_imgui11(); // 1.1 Implementation
    init_imgui13();

    init_default_data();

    // initialise entity component systems
    //init_ecs();

    Fish::Timer::EngineTimer _timer;
    m_EngineTimer = _timer;

    // everything went fine
    m_IsInitialized = true;
}

void FishVulkanEngine::init_vulkan()
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

void FishVulkanEngine::init_imgui11()
{
    //1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &m_ImGuiDescriptorPool));

    // 2: initialize imgui library

    //this initializes the core structures of imgui
    ImGui::CreateContext();

    //this initializes imgui for SDL
    bool ret = true;
    ret = ImGui_ImplSDL2_InitForVulkan(m_pWindow);
    assert(ret && "Failed to initialise ImGui for SDL2.\n");

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_VkInstance;
    init_info.PhysicalDevice = m_PhysicalDevice;
    init_info.Device = m_Device;
    init_info.Queue = m_GraphicsQueue;
    init_info.DescriptorPool = m_ImGuiDescriptorPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, m_MainRenderPass);

    //execute a gpu command to upload imgui font textures
    VkCommandBuffer cmd = get_current_frame().m_CommandBuffer;

    immediate_submit11([=](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    //add the destroy the imgui created structures
    m_DeletionQueue.push_function([=]() {
        vkDestroyDescriptorPool(m_Device, m_ImGuiDescriptorPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    });
}

void FishVulkanEngine::init_imgui13()
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

void FishVulkanEngine::init_swapchain()
{
    create_swapchain(m_WindowExtents.width, m_WindowExtents.height);
}

void FishVulkanEngine::init_commands()
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

void FishVulkanEngine::init_main_renderpass()
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

void FishVulkanEngine::init_framebuffers()
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

void FishVulkanEngine::init_descriptors11()
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
    const size_t sceneParamBufferSize = kFrameOverlap * pad_uniform_buffer_size(sizeof(Fish::GPU::SceneData));
    m_SceneParametersBuffer = create_buffer11(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //

    for (int i = 0; i < kFrameOverlap; ++i)
    {
        // Camera buffer.
        m_Frames[i].cameraBuffer = create_buffer11(sizeof(Fish::GPU::CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Object buffer.
        const int kMaxObjects = 10000;
        m_Frames[i].objectBuffer = create_buffer11(sizeof(Fish::GPU::ObjectData) * kMaxObjects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

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
        cameraInfo.range = sizeof(Fish::GPU::CameraData);

        // Scene info.
        VkDescriptorBufferInfo sceneInfo;
        sceneInfo.buffer = m_SceneParametersBuffer.buffer;
        sceneInfo.offset = 0;
        sceneInfo.range = sizeof(Fish::GPU::SceneData);

        // Object info.
        VkDescriptorBufferInfo objectInfo;
        objectInfo.buffer = m_Frames[i].objectBuffer.buffer;
        objectInfo.offset = 0;
        objectInfo.range = sizeof(Fish::GPU::ObjectData) * kMaxObjects;

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

//void FishVulkanEngine::init_pipelines11()
//{
//    // TODO: Load shaders in one function, and shorten the function into a "check_load" type function.
//
//    VkShaderModule colorMeshShader;
//    if (!load_shader_module("../../shaders/default_lit.frag.spv", &colorMeshShader))
//    {
//        fmt::println("Error when building the defaultLitShader shader module");
//    }
//    else {
//        fmt::println("defaultLitShader shader successfully loaded");
//    }
//
//    VkShaderModule texturedMeshShader;
//    if (!load_shader_module("../../shaders/textured_lit.frag.spv", &texturedMeshShader))
//    {
//        fmt::println("Error when building the texturedLitShader shader module");
//    }
//    else {
//        fmt::println("texturedLitShader shader successfully loaded");
//    }
//
//    VkShaderModule meshVertShader;
//    if (!load_shader_module("../../shaders/triangleMesh.vert.spv", &meshVertShader))
//    {
//        fmt::println("Error when building the triangleMeshShader shader module");
//    }
//    else {
//        fmt::println("triangleMeshShader shader successfully loaded");
//    }
//      
//
//    //build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
//    Fish::PipelineBuilder11 pipelineBuilder;   
//
//    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
//    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colorMeshShader));
//    
//    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
//    VkPushConstantRange push_constant;
//    push_constant.offset = 0;
//    push_constant.size = sizeof(MeshPushConstants);
//    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//    mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
//    mesh_pipeline_layout_info.pushConstantRangeCount = 1; 
//    VkDescriptorSetLayout setLayouts[] = { m_GlobalSetLayout, m_ObjectSetLayout };
//    mesh_pipeline_layout_info.setLayoutCount = 2;
//    mesh_pipeline_layout_info.pSetLayouts = setLayouts;
//    
//    // Create the mesh pipeline layout.
//    VkPipelineLayout meshPipLayout;
//    VK_CHECK(vkCreatePipelineLayout(m_Device, &mesh_pipeline_layout_info, nullptr, &meshPipLayout));
//
//    // Change necessary values for creation of the texture pipeline layout.
//    VkPipelineLayoutCreateInfo textured_pipeline_layout_info = mesh_pipeline_layout_info;
//    VkDescriptorSetLayout texturedSetLayouts[] = { m_GlobalSetLayout, m_ObjectSetLayout, m_SingleTextureSetLayout };
//    textured_pipeline_layout_info.setLayoutCount = 3;
//    textured_pipeline_layout_info.pSetLayouts = texturedSetLayouts;
//    VkPipelineLayout texturedPipeLayout;
//    VK_CHECK(vkCreatePipelineLayout(m_Device, &textured_pipeline_layout_info, nullptr, &texturedPipeLayout));
//
//    pipelineBuilder.pipelineLayout = meshPipLayout;
//    pipelineBuilder.vertexInputState = vkinit::vertex_input_state_create_info(); // how to read vertices from vertex buffers
//    pipelineBuilder.inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
//    //build viewport and scissor from the swapchain extents
//    pipelineBuilder.viewport.x = 0.0f;
//    pipelineBuilder.viewport.y = 0.0f;
//    pipelineBuilder.viewport.width = (float)m_WindowExtents.width;
//    pipelineBuilder.viewport.height = (float)m_WindowExtents.height;
//    pipelineBuilder.viewport.minDepth = 0.0f;
//    pipelineBuilder.viewport.maxDepth = 1.0f;
//    pipelineBuilder.scissor.offset = { 0, 0 };
//    pipelineBuilder.scissor.extent = m_WindowExtents;
//    //configure the rasterizer to draw filled triangles
//    pipelineBuilder.rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
//    //we dont use multisampling, so just run the default one
//    pipelineBuilder.multisampler = vkinit::multisampling_state_create_info();
//    //a single blend attachment with no blending and writing to RGBA
//    pipelineBuilder.colourBlendAttachment = vkinit::color_blend_attachment_state();
//    //default depthtesting
//    pipelineBuilder.depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
//
//    //connect the pipeline builder vertex input info to the one we get from Vertex
//    Fish::Resource::VertexInputDescription vertexDescription = Fish::Resource::Vertex::get_vertex_description();
//    pipelineBuilder.vertexInputState.pVertexAttributeDescriptions = vertexDescription.attributes.data();
//    pipelineBuilder.vertexInputState.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
//    pipelineBuilder.vertexInputState.pVertexBindingDescriptions = vertexDescription.bindings.data();
//    pipelineBuilder.vertexInputState.vertexBindingDescriptionCount = vertexDescription.bindings.size();
//
//    //build the mesh triangle pipeline
//    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);
//    Fish::ResourceManager::Get().create_material(meshPipeline, meshPipLayout, "defaultmesh");
//
//    pipelineBuilder.shaderStages.clear();
//    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
//    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, texturedMeshShader));
//
//    pipelineBuilder.pipelineLayout = texturedPipeLayout;
//    VkPipeline texPipeline = pipelineBuilder.build_pipeline(m_Device, m_MainRenderPass);
//    Fish::ResourceManager::Get().create_material(texPipeline, texturedPipeLayout, "texturedmesh");
//
//    vkDestroyShaderModule(m_Device, meshVertShader, nullptr);
//    vkDestroyShaderModule(m_Device, colorMeshShader, nullptr);
//    vkDestroyShaderModule(m_Device, texturedMeshShader, nullptr);
//
//    m_DeletionQueue.push_function([=]() {
//        vkDestroyPipeline(m_Device, meshPipeline, nullptr);
//        vkDestroyPipeline(m_Device, texPipeline, nullptr);
//
//        vkDestroyPipelineLayout(m_Device, meshPipLayout, nullptr);
//        vkDestroyPipelineLayout(m_Device, texturedPipeLayout, nullptr);
//    });
//}

void FishVulkanEngine::init_pipelines13()
{
    // Compute Pipelines.
    init_background_pipelines();

    // Graphics Pipelines.
    init_triangle_pipeline();
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

void FishVulkanEngine::init_triangle_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle.frag.spv", m_Device, &triangleFragShader)) {
        fmt::print("Error when building the triangle fragment shader module");
    }
    else {
        fmt::print("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle.vert.spv", m_Device, &triangleVertexShader)) {
        fmt::print("Error when building the triangle vertex shader module");
    }
    else {
        fmt::print("Triangle vertex shader succesfully loaded");
    }

    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(m_Device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

    // Create the pipeline using our builder abstraction.

    Fish::PipelineBuilder13 pipelineBuilder;

    //use the triangle layout we created
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
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
    //no depth testing
    pipelineBuilder.disable_depthtest();

    //connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(m_DrawImage.imageFormat);
    pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

    //finally build the pipeline
    _trianglePipeline = pipelineBuilder.build_pipeline(m_Device);

    //clean structures
    vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
    vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);

    m_DeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(m_Device, _trianglePipelineLayout, nullptr);
        vkDestroyPipeline(m_Device, _trianglePipeline, nullptr);
    });
}

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
    pipelineBuilder.disable_blending();

    pipelineBuilder.disable_depthtest();

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

void FishVulkanEngine::init_synchronisation_structures()
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

    // draw image size will match the window
        VkExtent3D drawImageExtent = {
            m_WindowExtents.width,
            m_WindowExtents.height,
            1
    };

    //hardcoding the draw format to 32 bit float
    m_DrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_DrawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

    m_DeletionQueue.push_function([=]() {
        vkDestroyImageView(m_Device, m_DrawImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);
    });

    //
    // 
    // 
    // 
    // 
    
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

void FishVulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < m_SwapchainImageViews.size(); i++) {

        vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
    }
}

// TODO: Comment and refactor.
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

void FishVulkanEngine::init_descriptors13()
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

// Deprecated and refactored.
void FishVulkanEngine::render()
{
    // 1 second
    const unsigned int timeout = 1000000000;

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(m_Device, 1, &get_current_frame().m_RenderFence, true, timeout));

    //get_current_frame().deletionQueue.flush();

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
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // > draw first
    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    draw_background(cmd);
    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // < draw first

    //prepare the submission to the queue. 
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().m_PresentSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().m_RenderSemaphore);
    VkSubmitInfo2 submit = vkinit::submit_info2(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, get_current_frame().m_RenderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame().m_RenderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;
    VkResult presentResult = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

    //
    //
    //

    //// clear at a depth of 1
    //VkClearValue depthClear;
    //depthClear.depthStencil.depth = 1.0f;

    ////start the main renderpass.
    ////We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    //VkRenderPassBeginInfo rpInfo = {};
    //rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //rpInfo.pNext = nullptr;
    //rpInfo.renderPass = m_MainRenderPass;
    //rpInfo.renderArea.offset.x = 0;
    //rpInfo.renderArea.offset.y = 0;
    //rpInfo.renderArea.extent = m_WindowExtents;
    //rpInfo.framebuffer = m_FrameBuffers[swapchainImageIndex];

    ////connect clear values
    ////VkClearValue clearValues[] = { clearValue, depthClear };
    ////rpInfo.clearValueCount = 2;
    ////rpInfo.pClearValues = &clearValues[0];

    //vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    ////
    //// 
    //// Actually send out rendering commands:
    //// 
    //// 
    //
    //// Object Render Pass.

    //std::vector<Fish::Resource::RenderObject> renderable = Fish::ResourceManager::Get().m_Scene.m_SceneObjects;
    //render_objects(cmd, renderable.data(), renderable.size());

    //// Kind of ImGui Render Pass.
    //ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    ////finalize the render pass
    //vkCmdEndRenderPass(cmd);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));   

    //increase the number of frames drawn
    m_FrameNumber++;
}

//void FishVulkanEngine::render_imgui()
//{
//    // Configuration.
//    ImGuiIO io = ImGui::GetIO();
//
//    // Style.
//    ImGuiStyle* style = &ImGui::GetStyle();
//    style->FrameBorderSize = 1;
//
//    // New Frame.
//    ImGui_ImplVulkan_NewFrame();
//    ImGui_ImplSDL2_NewFrame(m_pWindow);
//    ImGui::NewFrame();
//
//    //ImGui::ShowDemoWindow();
//
//    // Begin Debug Overlay
//    ImGui::Begin("Debug Overlay", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar);
//    imgui_debug_data();
//    ImGui::End(); // Debug Overlay
//
//    // Begin Object Hierarchy
//    ImGui::Begin("Hierarchy", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
//    imgui_object_hierarchy();
//    ImGui::End(); // Object Hierarchy
//
//    // Begin Scene Data
//    ImGui::Begin("Scene", (bool*)0, ImGuiWindowFlags_None);
//    imgui_scene_data();
//    ImGui::End(); // Scene Data
//
//
//    //
//    //
//    //
//
//    ImGui::Render(); // Must either be at the very end of this function, or within the render loop (ideally at the beginning).
//}

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

    // Begin Object Hierarchy
    /*{
        ImGui::Begin("Hierarchy", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
        imgui_object_hierarchy();
        ImGui::End(); 
    }*/
    // End Object Hierarchy

    // Begin Scene Data
    /*{
        ImGui::Begin("Scene", (bool*)0, ImGuiWindowFlags_None);
        imgui_scene_data();
        ImGui::End();
    }*/
    // End Scene Data
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

GPUMeshBuffers FishVulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    //create vertex buffer
    newSurface.vertexBuffer = create_buffer13(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_Device, &deviceAdressInfo);

    //create index buffer
    newSurface.indexBuffer = create_buffer13(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer13 staging = create_buffer13(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
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

void FishVulkanEngine::init_default_data()
{
    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = { 0.5,-0.5, 0 };
    rect_vertices[1].position = { 0.5,0.5, 0 };
    rect_vertices[2].position = { -0.5,-0.5, 0 };
    rect_vertices[3].position = { -0.5,0.5, 0 };

    rect_vertices[0].color = { 0,0, 0,1 };
    rect_vertices[1].color = { 0.5,0.5,0.5 ,1 };
    rect_vertices[2].color = { 1,0, 0,1 };
    rect_vertices[3].color = { 0,1, 0,1 };

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle = uploadMesh(rect_indices, rect_vertices);

    //delete the rectangle data on engine shutdown
    m_DeletionQueue.push_function([&]() {
        destroy_buffer(rectangle.indexBuffer);
        destroy_buffer(rectangle.vertexBuffer);
    });

    // load test mesh
    testMeshes = loadGltfMeshes(loadedEngine, "..//..//assets//basicmesh.glb").value();
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
    //wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(m_Device, 1, &get_current_frame().m_RenderFence, true, 1000000000));

    get_current_frame().deletionQueue.flush();

    //request image from the swapchain
    uint32_t swapchainImageIndex;

    VkResult e = vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, get_current_frame().m_PresentSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        rebuild_swapchain();
        return;
    }

    VK_CHECK(vkResetFences(m_Device, 1, &get_current_frame().m_RenderFence));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(get_current_frame().m_CommandBuffer, 0));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame().m_CommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    //> draw_first
    m_DrawExtent.width = m_DrawImage.imageExtent.width;
    m_DrawExtent.height = m_DrawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    //transtion the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    //< draw_first
    //> imgui_draw
        // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_SwapchainExtent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
    draw_imgui(cmd, m_SwapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));
    //< imgui_draw

        //prepare the submission to the queue. 
        //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
        //we will signal the _renderSemaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().m_PresentSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info2(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().m_RenderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info2(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, get_current_frame().m_RenderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = vkinit::present_info();

    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame().m_RenderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);


    //increase the number of frames drawn
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

        m_EngineTimer.tick();

        //update(m_EngineTimer.delta_time());

        // Set all draw data for ImGui so that the render loop can submit this data.
        prepare_imgui();        

        // Main draw loop.
        draw();
    }
}

//void FishVulkanEngine::render_objects(VkCommandBuffer cmd, Fish::Resource::RenderObject* first, int count)
//{
//    //make a model view matrix for rendering the object
//    //camera view
//    m_Camera.update();
//
//    glm::mat4 view = glm::translate(glm::mat4(1.f), m_Camera.m_Position);
//    view = m_Camera.get_view_matrix();
//
//    //camera projection
//    glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f); // todo remove magic numbers
//    projection[1][1] *= -1;
//
//    //fill a GPU camera data struct
//    Fish::GPU::CameraData camData;
//    camData.projectionMatrix = projection;
//    camData.viewMatrix = view;
//    camData.viewProjectionMatrix = projection * view; // matrix multiplication is reversed.
//
//    //and copy it to the buffer
//    void* data;
//    vmaMapMemory(m_Allocator, get_current_frame().cameraBuffer.allocation, &data);
//    memcpy(data, &camData, sizeof(Fish::GPU::CameraData));
//    vmaUnmapMemory(m_Allocator, get_current_frame().cameraBuffer.allocation);
//
//    // write into the scene buffer
//    float framed = (m_FrameNumber / 120.0f);
//    m_SceneParameters.ambientColour = { sin(framed), 0, cos(framed), 1 };
//    char* sceneData;
//    vmaMapMemory(m_Allocator, m_SceneParametersBuffer.allocation, (void**)&sceneData);
//    int frameIndex = m_FrameNumber & kFrameOverlap;
//    sceneData += pad_uniform_buffer_size(sizeof(Fish::GPU::SceneData)) * frameIndex;
//    memcpy(sceneData, &m_SceneParameters, sizeof(Fish::GPU::SceneData));
//    vmaUnmapMemory(m_Allocator, m_SceneParametersBuffer.allocation);    
//
//    // write into the object buffer (shader storage buffer)
//    void* objectData;
//    vmaMapMemory(m_Allocator, get_current_frame().objectBuffer.allocation, &objectData);
//    Fish::GPU::ObjectData* objectSSBO = (Fish::GPU::ObjectData*)objectData;
//    for (int i = 0; i < count; i++)
//    {
//        Fish::Resource::RenderObject& object = first[i];
//        objectSSBO[i].modelMatrix = object.transformMatrix;
//    }
//    vmaUnmapMemory(m_Allocator, get_current_frame().objectBuffer.allocation);
//
//    Fish::Resource::Mesh* lastMesh = nullptr;
//    Fish::Resource::Material* lastMaterial = nullptr;
//    for (int i = 0; i < count; i++)
//    {
//        Fish::Resource::RenderObject& object = first[i];
//
//        //only bind the pipeline if it doesn't match with the already bound one
//        if (object.pMaterial != lastMaterial) {
//
//            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipeline);
//            lastMaterial = object.pMaterial;
//
//            //offset for our scene buffer
//            uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(Fish::GPU::SceneData)) * frameIndex;
//            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipelineLayout, 0, 1, &get_current_frame().globalDescriptor, 1, &uniform_offset);
//
//            //object data descriptor
//            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipelineLayout, 1, 1, &get_current_frame().objectDescriptor, 0, nullptr);
//
//            //texture descriptor
//            if (object.pMaterial->textureSet != VK_NULL_HANDLE) {
//                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.pMaterial->pipelineLayout, 2, 1, &object.pMaterial->textureSet, 0, nullptr);
//            }
//
//        }
//
//        glm::mat4 model = object.transformMatrix;
//
//        //final render matrix, that we are calculating on the cpu
//        //glm::mat4 mesh_matrix = projection * view * model;
//
//        MeshPushConstants constants;
//        constants.matrix = object.transformMatrix;
//
//        //upload the mesh to the GPU via push constants
//        vkCmdPushConstants(cmd, object.pMaterial->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);
//
//        //bind the mesh buffers with offset 0
//        VkDeviceSize vertexOffset = 0, indexOffset = 0;
//
//        //only bind the mesh if it's a different one from last bind
//        if (object.pMesh != lastMesh) {
//            vkCmdBindVertexBuffers(cmd, 0, 1, &object.pMesh->vertexBuffer.buffer, &vertexOffset);
//            vkCmdBindIndexBuffer(cmd, object.pMesh->indexBuffer.buffer, indexOffset, VK_INDEX_TYPE_UINT32);
//            lastMesh = object.pMesh;
//        }
//
//        //we can now draw - this may or may not cause an error doing both of these here
//        //if you are stuck rendering and see this, try one or the other of the below lines instead of both.
//        vkCmdDraw(cmd, object.pMesh->vertices.size(), 1, 0, i);                         // Draw via vertices.
//        vkCmdDrawIndexed(cmd, object.pMesh->indices.size(), 1, 0, vertexOffset, i);     // Draw via indicies and a vertex offset.
//    }
//}

void FishVulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    //begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);

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

    //launch a draw command to draw 3 vertices
    vkCmdDraw(cmd, 3, 1, 0, 0); // cmd, vertex count, instance count, first vertex, first instance

    // > draw rectangle.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = glm::mat4{ 1.f };
    push_constants.vertexBuffer = rectangle.vertexBufferAddress;

    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0); // cmd, index count, instance count, first index, vertex offset, first instance
    // < draw rectangle.

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

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DrawImageDescriptors, 0, nullptr);

    // Submit the push constants to the GPU to be accessible within the shader.
    vkCmdPushConstants(cmd, m_GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
}

void FishVulkanEngine::init_ecs()
{
    // initalise the coordinator to connect all parts of the ecs
    m_Ecs->Init();

    // register the components that are going to be used by entities
    m_Ecs->RegisterComponent<Fish::Component::Transform>();
    m_Ecs->RegisterComponent<Fish::Component::RigidBody>();

    m_PhysicsSystem = m_Ecs->RegisterSystem<Fish::ECS::System::Physics>();

    // create and set the component signatures so the system knows what components to be updating
    Fish::ECS::Signature signature;
    //signature.reset();

    // set up for the physics system
    signature.set(m_Ecs->GetComponentType<Fish::Component::Transform>());
    signature.set(m_Ecs->GetComponentType<Fish::Component::RigidBody>());
    m_Ecs->SetSystemSignature<Fish::ECS::System::Physics>(signature);
    m_PhysicsSystem->init(m_Ecs);

    // set up for the ... 
    //signature.reset();
    // ...
}

FrameData& FishVulkanEngine::get_current_frame()
{
    return m_Frames[m_FrameNumber % kFrameOverlap];
}

AllocatedBuffer11 FishVulkanEngine::create_buffer11(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;

    AllocatedBuffer11 newBuffer;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, nullptr));

    return newBuffer;
}

AllocatedBuffer13 FishVulkanEngine::create_buffer13(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
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

void FishVulkanEngine::update(float deltatime)
{
    //m_PhysicsSystem->update(deltatime);
}
