// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

// This will be the main class for the engine, and where most of the code of the vulkan guide tutorial will go.

#pragma once

#include <vk_types.h>

#include <fish_timer.h>
#include <fish_resource_manager.h>

#include <unordered_map>
#include <fish_camera.h>
#include <fish_gpu_data.h>

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

	// ... 
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	// returns by value
	VmaAllocator GetAllocator() { return m_Allocator; }

	// returns a reference
	DeletionQueue& GetDeletionQueue() { return m_DeletionQueue; }
	VkDevice& GetDevice() { return m_Device; } 
	VkDescriptorPool& GetDescriptorPool() { return m_DescriptorPool; }
	VkDescriptorSetLayout& GetSingleTextureSetLayout() { return m_SingleTextureSetLayout; }

private:

	// bet you cant guess what these functions do...
	void init_vulkan();
	void init_imgui(); 
	void init_swapchain();
	void init_commands();
	void init_main_renderpass();
	void init_framebuffers();
	void init_synchronisation_structures(); // for fences and semaphores
	void init_descriptors();
	void init_pipelines();
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	// loads a shader module from a spir-v file. false if error.
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	//draw loop responsible for synchronisation and the recording of command buffers.
	void render();

	//draw loop responsible for rendering scene objects.
	void render_objects(VkCommandBuffer cmd, Fish::Resource::RenderObject* first, int count);
	
	// commands responsible for rendering imgui.
	void render_imgui();

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

	// Descriptor Set.
	VkDescriptorSetLayout m_SingleTextureSetLayout;						// ...
	VkDescriptorSetLayout m_GlobalSetLayout;							// ...
	VkDescriptorSetLayout m_ObjectSetLayout;							// ...
	VkDescriptorPool m_DescriptorPool;									// ...
	
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
};
