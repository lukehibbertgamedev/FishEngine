// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

// This will be the main class for the engine, and where most of the code of the vulkan guide tutorial will go.

#pragma once

#include <vk_types.h>

#include <fish_timer.h>
#include <fish_resource_manager.h>
#include <fish_ecs.h>
#include <fish_ecs_systems.h>

#include <unordered_map>
#include <fish_camera.h>
#include <fish_gpu_data.h>
#include <vk_descriptors.h>

// safely handle the cleanup of a growing amount of objects.
struct DeletionQueue {

	std::deque<std::function<void()>> toDelete;

	void push_function(std::function<void()>&& function) {
		toDelete.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = toDelete.rbegin(); it != toDelete.rend(); it++) {
			(*it)(); //call functors
		}

		toDelete.clear();
	}
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 matrix;
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

// Editable parameters.
struct ComputeEffect {
	const char* name;
	VkPipeline pipeline;
	VkPipelineLayout layout;
	ComputePushConstants data;
};

struct UploadContext {
	VkFence uploadFence;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
};

struct FrameData {
	VkCommandPool m_CommandPool;										// A collection of command buffers. Resetting the command pool resets all command buffers allocated from it.
	VkCommandBuffer m_CommandBuffer;									// All commands are recorded into a command buffer, this won't do anything unless submitted to the GPU.
	VkFence m_RenderFence;												// Used for GPU -> CPU communication.
	VkSemaphore m_PresentSemaphore, m_RenderSemaphore;					// Used for GPU -> GPU synchronisation.

	AllocatedBuffer cameraBuffer;										// Buffer holding a single GPUCameraData to use during rendering.
	VkDescriptorSet globalDescriptor;									// Holds the matrices that we need.

	AllocatedBuffer objectBuffer;										//
	VkDescriptorSet objectDescriptor;									// Holds the matrices that we need.

	DeletionQueue deletionQueue;										// Allows the deletion of objects within the next frame after use.
};

class FishVulkanEngine {
public:

	// Singleton instance.
	static FishVulkanEngine& Get();

	//initializes everything in the engine
	void init();	

	//shuts down the engine
	void cleanup();	

	//run main loop
	void run();

	// abstracted buffer creation.
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	// 1.1 - Send commands to the GPU without synchronisation with the swapchain or rendering logic.
	void immediate_submit11(std::function<void(VkCommandBuffer cmd)>&& function);
	// 1.3 - Send commands to the GPU without synchronisation with the swapchain or rendering logic.
	void immediate_submit13(std::function<void(VkCommandBuffer cmd)>&& function);

	VmaAllocator GetAllocator() { return m_Allocator; }											 // By value accessor.

	DeletionQueue& GetDeletionQueue() { return m_DeletionQueue; }								 // By reference accessor.
	VkDevice& GetDevice() { return m_Device; } 													 // By reference accessor.
	VkDescriptorPool& GetDescriptorPool() { return m_DescriptorPool; }							 // By reference accessor.
	VkDescriptorSetLayout& GetSingleTextureSetLayout() { return m_SingleTextureSetLayout; }		 // By reference accessor.

private:

	// bet you cant guess what these functions do...
	void init_vulkan();

	void init_imgui11(); // 1.1 Implementation
	void init_imgui13();

	void init_swapchain();
	void init_commands();
	void init_main_renderpass();
	void init_framebuffers();
	void init_synchronisation_structures(); // for fences and semaphores
	
	void init_descriptors11(); // 1.1 Implementation
	void init_descriptors13();

	void init_pipelines11(); // 1.1 Implementation
	void init_pipelines13();
	void init_background_pipelines();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void rebuild_swapchain();

	// loads a shader module from a spir-v file. false if error.
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	void update(float deltatime);

	// 1.1 - Main draw loop for sycnhronisation and recording command buffers.
	void render();
	// 1.3 - Main draw loop for sycnhronisation and recording command buffers.
	void draw();

	// 1.1 - Draw loop responsible for rendering scene objects.
	void render_objects(VkCommandBuffer cmd, Fish::Resource::RenderObject* first, int count);

	// 1.3 Draw loop responsible for rendering the clear value.
	void draw_background(VkCommandBuffer cmd);
	
	// set up all entity component systems
	void init_ecs();

	// 1.1 - Commands responsible for rendering imgui.
	void render_imgui();

	// 1.3 - Set up ImGui backends and create draw data.
	void prepare_imgui();
	// 1.3 - Call ImGui functions to add to the draw data.
	void create_imgui_draw_data();
	// 1.3 - Submit the commands for rendering the ImGui draw data.
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	void imgui_debug_data();
	void imgui_object_hierarchy();
	void imgui_scene_data();

	// return frame we are rendering to right now.
	FrameData& get_current_frame();

	// pad the size of something to the alignment boundary
	// with thanks to https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
	size_t pad_uniform_buffer_size(size_t originalSize);

	// ------------------------------------------------------------------------------------------------------------------------------------------------------------ // 
	
	// Window.
	VkExtent2D m_WindowExtents{ 1700 , 900 };							// Size of the window we are going to open (in pixels).
	struct SDL_Window* m_pWindow{ nullptr };							// A pointer to our window, using SDL2 (notably a forward-declaration).

	// Flags.
	bool m_IsInitialized{ false };										// A flag to check if the engine has been initialised.
	bool m_StopRendering{ false };										// A flag to check if the engine should render the image (e.g., no if window is minimized).

	// Frames.
	FrameData m_Frames[kFrameOverlap];									// A handle to our frame data using either double/triple-buffering (depending on kFrameOverlap).
	std::vector<VkFramebuffer> m_FrameBuffers;							// A link between the attachments of the render pass and the real images they render to.
	int m_FrameNumber{ 0 };												// Frame number integer to determine the number of frames passed (i assume...).	

	// Vulkan.
	VkInstance m_VkInstance;											// Vulkan library handle.
	VkDebugUtilsMessengerEXT m_DebugMessenger;							// Vulkan debug output handle.
	VkPhysicalDevice m_PhysicalDevice;									// GPU chosen as the default device.
	VkDevice m_Device;													// Vulkan device for commands.
	VkSurfaceKHR m_SurfaceKHR;											// Vulkan window surface.

	// Swapchain.
	VkSwapchainKHR m_Swapchain;											// Swapchain handle.
	VkFormat m_SwapchainImageFormat;									// Cached image format to reuse.
	std::vector<VkImage> m_SwapchainImages;								// A vector of images that the swapchain will swap between (i.e. double/triple buffering).
	std::vector<VkImageView> m_SwapchainImageViews;						// Metadata for the respective swapchain image.
	VkExtent2D m_SwapchainExtent;										// The size of the swapchain image.

	// Allocation.
	VmaAllocator m_Allocator;											// All buffers and images must be created on top of memory allocation. VMA abstracts this for us.
	
	// Graphics.
	VkQueue m_GraphicsQueue;											// An execution port for the GPU to stream graphics commands.
	uint32_t m_GraphicsQueueFamily;										// Defines the type of queue and the type of commands the queue supports.
	VkPhysicalDeviceProperties m_GPUProperties;							// ...

	// Depth.
	AllocatedImage m_DepthImage;										// Depth image handle to configure z-testing with a depth buffer.
	VkImageView m_DepthImageView;										// Metadata for the depth image.
	VkFormat m_DepthFormat;												// Cached format of the depth image for reuse.

	// ...
	AllocatedImage m_DrawImage;
	VkExtent2D m_DrawExtent;

	// Descriptor Set.
	VkDescriptorSetLayout m_SingleTextureSetLayout;						// ...
	VkDescriptorSetLayout m_GlobalSetLayout;							// ...
	VkDescriptorSetLayout m_ObjectSetLayout;							// ...
	VkDescriptorPool m_DescriptorPool;									// ...

	DescriptorAllocator m_GlobalDescriptorAllocator;
	VkDescriptorSet m_DrawImageDescriptors;
	VkDescriptorSetLayout m_DrawImageDescriptorLayout;
	
	// Scene Data.
	Fish::GPU::SceneData m_SceneParameters;								// ...
	AllocatedBuffer m_SceneParametersBuffer;							// ...
	Fish::Camera m_Camera;												// A handle to our camera so we can move around our scene (expanded to current camera in future).

	// ImGui.
	VkDescriptorPool m_ImGuiDescriptorPool;								// ... Descriptor pool info but one specific for ImGui.

	// Misc...
	Fish::Timer::EngineTimer m_EngineTimer;								// A handle to our timer class so that we can calculate certain frame metrics.
	UploadContext m_UploadContext;										// ... 
	DeletionQueue m_DeletionQueue;										// More efficient implementation of a deletion/cleanup system. Uses a FIFO order (good for small engines).
	VkRenderPass m_MainRenderPass;										// All rendering happens here. Begin and End render pass are recorded into the command buffer for other rendering commands between.

	std::shared_ptr<Fish::ECS::Coordinator> m_Ecs; // m_EntityComponentSystemCoordinator
	std::shared_ptr<Fish::ECS::System::Physics> m_PhysicsSystem;

	VkPipeline m_GradientPipeline;
	VkPipelineLayout m_GradientPipelineLayout;

	// These are required for use of immediate GPU commands. An immediate_submit function will
	// use a fence and a different command buffer from the one we use on draws to send commands to
	// the GPU without synchronising with the swapchain or rendering logic.
	VkFence m_ImmediateFence;
	VkCommandBuffer m_ImmediateCommandBuffer;
	VkCommandPool m_ImmediateCommandPool;

	//

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };
};
