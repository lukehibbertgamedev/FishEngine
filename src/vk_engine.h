// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

// This will be the main class for the engine, and where most of the code of the vulkan guide tutorial will go.

#pragma once

#include <vk_types.h>

class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkPipelineVertexInputStateCreateInfo vertexInputState;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState colourBlendAttachment;
	VkPipelineMultisampleStateCreateInfo multisampler;
	VkPipelineLayout pipelineLayout;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass renderPass);
};

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

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

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkFence renderFence;
	VkSemaphore presentSemaphore, renderSemaphore;
	DeletionQueue deletionQueue;
};

constexpr unsigned int kFrameOverlap = 2;

class VulkanEngine {
public:

	// TODO: refactor to an enum using index and filename.
	int m_SelectedShader{ 0 }; // 0 for red triangle. 1 for coloured triangle.

	VkPipeline m_RedTrianglePipeline;
	VkPipeline m_ColouredTrianglePipeline;
	VkPipelineLayout m_TrianglePipelineLayout;

	AllocatedImage m_DrawImage;
	/*VkExtent2D m_DrawExtent;*/

	VkRenderPass m_MainRenderPass;
	std::vector<VkFramebuffer> m_FrameBuffers;

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkFence renderFence;
	VkSemaphore presentSemaphore, renderSemaphore;
	DeletionQueue deletionQueue;

	//FrameData m_Frames[kFrameOverlap];

	//FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % kFrameOverlap]; }

	VkQueue m_GraphicsQueue;
	uint32_t m_GraphicsQueueFamily;

	// A flag to check if the engine has been initialised.
	bool m_IsInitialized{ false };

	// Frame number integer to determine the number of frames passed (i assume...)
	int m_FrameNumber {0};

	// todo
	bool m_StopRendering{ false };

	// Size of the window we are going to open (in pixels).
	VkExtent2D m_WindowExtents{ 1700 , 900 };

	// A pointer to our window, using SDL2 (notably a forward-declaration).
	struct SDL_Window* m_pWindow{ nullptr };

	// Singleton instance.
	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_main_renderpass();
	void init_framebuffers();
	void init_pipelines();

	void init_synchronisation_structures();

	// loads a shader module from a spir-v file. false if error.
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();
	void draw_background(VkCommandBuffer cmd);

	//run main loop
	void run();

private:

	VkInstance m_VkInstance;// Vulkan library handle
	VkDebugUtilsMessengerEXT m_DebugMessenger;// Vulkan debug output handle
	VkPhysicalDevice m_PhysicalDevice; // GPU chosen as the default device
	VkDevice m_Device; // Vulkan device for commands
	VkSurfaceKHR m_SurfaceKHR;// Vulkan window surface

	VkSwapchainKHR m_Swapchain;
	VkFormat m_SwapchainImageFormat;
	std::vector<VkImage> m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;
	VkExtent2D m_SwapchainExtent;
};
