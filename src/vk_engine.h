// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

// This will be the main class for the engine, and where most of the code of the vulkan guide tutorial will go.

#pragma once

#include <vk_types.h>

#include <fish_timer.h>
#include <fish_resource_manager.h>

#include <unordered_map>

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
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineLayout pipelineLayout;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass renderPass);
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


enum SelectedShader {

	ColouredPipeline = 0,
	RedPipeline = 1,
	MeshPipeline = 2,
	MaxPipelines
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 matrix;
};

//struct Material {
//	VkDescriptorSet textureSet{ VK_NULL_HANDLE }; //texture defaulted to null
//	VkPipeline pipeline;
//	VkPipelineLayout pipelineLayout;
//};

//struct RenderObject {	
//	Fish::Resource::Mesh* pMesh;
//	Fish::Resource::Material* pMaterial;
//	glm::mat4 transformMatrix;
//};

struct UploadContext {
	VkFence uploadFence;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
};

constexpr unsigned int kFrameOverlap = 2;

struct GPUCameraData {
	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;
	glm::mat4 viewProjectionMatrix;
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

struct GPUObjectData {
	glm::mat4 modelMatrix;												
};

struct GPUSceneData {
	glm::vec4 fogColour;												// W is for exponent.
	glm::vec4 fogDistances;												// X for min. Y for max. ZW unused.
	glm::vec4 ambientColour;
	glm::vec4 sunlightDirection;										// W is for intensity/power.
	glm::vec4 sunlightColour;
};



class VulkanEngine {
public:

	// Singleton instance.
	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();	

	//shuts down the engine
	void cleanup();	

	//run main loop
	void run();

	// abstracted buffer creation.
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	VmaAllocator GetAllocator() { return m_Allocator; }

	// returns a reference
	DeletionQueue& GetDeletionQueue() { return m_DeletionQueue; }
	VkDevice& GetDevice() { return m_Device; }
	VkDescriptorPool& GetDescriptorPool() { return m_DescriptorPool; }
	VkDescriptorSetLayout& GetSingleTextureSetLayout() { return m_SingleTextureSetLayout; }

private:

	// bet you cant guess what these functions do...
	void init_vulkan();
	void init_imgui(); // disabled until chapter 5
	void init_swapchain();
	void init_commands();
	void init_main_renderpass();
	void init_framebuffers();
	void init_synchronisation_structures(); // for fences and semaphores
	void init_descriptors();
	void init_pipelines();
	//void init_scene();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	// loads a shader module from a spir-v file. false if error.
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	//draw loop responsible for synchronisation and the recording of command buffers.
	void render();

	// commands responsible for rendering imgui.
	void render_imgui();

	// sets up a mesh structure and uploads it (this is technically load_triangle())
	//void load_meshes();

	//void load_images();

	// allocate and map the mesh memory to a buffer
	//void upload_mesh(Fish::Resource::Mesh& mesh);

	//Fish::Resource::Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	// returns nullptr if cannot find
	//Fish::Resource::Material* get_material(const std::string& name);
	// returns nullptr if cannot find
	//Fish::Resource::Mesh* get_mesh(const std::string& name);

	void render_objects(VkCommandBuffer cmd, Fish::Resource::RenderObject* first, int count);

	// return frame we are rendering to right now.
	FrameData& get_current_frame();

	// pad the size of something to the alignment boundary
	// with thanks to https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
	size_t pad_uniform_buffer_size(size_t originalSize);

	
	VkExtent2D m_WindowExtents{ 1700 , 900 };							// Size of the window we are going to open (in pixels).
	struct SDL_Window* m_pWindow{ nullptr };							// A pointer to our window, using SDL2 (notably a forward-declaration).

	bool m_IsInitialized{ false };										// A flag to check if the engine has been initialised.
	bool m_StopRendering{ false };										// A flag to check if the engine should render the image (e.g., no if window is minimized).
	int m_FrameNumber{ 0 };												// Frame number integer to determine the number of frames passed (i assume...).	

	VkInstance m_VkInstance;											// Vulkan library handle.
	VkDebugUtilsMessengerEXT m_DebugMessenger;							// Vulkan debug output handle.
	VkPhysicalDevice m_PhysicalDevice;									// GPU chosen as the default device.
	VkDevice m_Device;													// Vulkan device for commands.
	VkSurfaceKHR m_SurfaceKHR;											// Vulkan window surface.

	VkSwapchainKHR m_Swapchain;											// Swapchain handle.
	VkFormat m_SwapchainImageFormat;									// Cached image format to reuse.
	std::vector<VkImage> m_SwapchainImages;								// A vector of images that the swapchain will swap between (i.e. double/triple buffering).
	std::vector<VkImageView> m_SwapchainImageViews;						// Metadata for the respective swapchain image.
	VkExtent2D m_SwapchainExtent;										// The size of the swapchain image.

	VmaAllocator m_Allocator;											// All buffers and images must be created on top of memory allocation. VMA abstracts this for us.
	
	VkQueue m_GraphicsQueue;											// An execution port for the GPU to stream graphics commands.
	uint32_t m_GraphicsQueueFamily;										// Defines the type of queue and the type of commands the queue supports.
	
	DeletionQueue m_DeletionQueue;										// More efficient implementation of a deletion/cleanup system. Uses a FIFO order (good for small engines).
	VkRenderPass m_MainRenderPass;										// All rendering happens here. Begin and End render pass are recorded into the command buffer for other rendering commands between.
	std::vector<VkFramebuffer> m_FrameBuffers;							// A link between the attachments of the render pass and the real images they render to.

	VkPipeline m_RedTrianglePipeline;									// A pipeline for the red triangle shader.
	VkPipeline m_ColouredTrianglePipeline;								// A pipeline for the use of colour in our triangle shader.
	VkPipeline m_MeshPipeline;											// A pipeline for a mesh version of our original triangle.

	VkPipelineLayout m_TrianglePipelineLayout;							// A full Vulkan object that contains all information about shader inputs for our triangle.
	VkPipelineLayout m_MeshPipelineLayout;								// A full Vulkan object that contains all information about shader inputs for our meshes.
	VkPipelineLayout m_TexturePipelineLayout;							// A full Vulkan object that contains all information about shader inputs for our textures.

	SelectedShader m_SelectedShader = SelectedShader::MeshPipeline;		// A way to determine which pipeline we are currently rendering.
	Fish::Resource::Mesh m_TriangleMesh;											// The current mesh we are working with (the triangle).
	Fish::Resource::Mesh m_MonkeyMesh;											// Obj loaded mesh.
	Fish::Resource::Mesh m_LostEmpireMesh;										// Obj loaded mesh.

	AllocatedImage m_DepthImage;										// Depth image handle to configure z-testing with a depth buffer.
	VkImageView m_DepthImageView;										// Metadata for the depth image.
	VkFormat m_DepthFormat;												// Cached format of the depth image for reuse.

	//std::vector<RenderObject> m_RenderObjects;							// All renderable objects.
	//std::unordered_map<std::string, Material> m_Materials;				// Map of name to material.
	//std::unordered_map<std::string, Fish::Resource::Mesh> m_Meshes;		// Map of name to mesh.
	//std::unordered_map<std::string, Fish::Resource::Texture> m_Textures;// Map of name to texture.

	FrameData m_Frames[kFrameOverlap];									//

	VkDescriptorSetLayout m_GlobalSetLayout;							//
	VkDescriptorSetLayout m_ObjectSetLayout;							//
	VkDescriptorPool m_DescriptorPool;									//
	
	VkPhysicalDeviceProperties m_GPUProperties;							// 

	GPUSceneData m_SceneParameters;										//
	AllocatedBuffer m_SceneParametersBuffer;							//

	UploadContext m_UploadContext;										//

	VkDescriptorSetLayout m_SingleTextureSetLayout;						//

	// ImGui section
	VkDescriptorPool m_ImGuiDescriptorPool;								// 

	Fish::Timer::EngineTimer timer;

	Fish::ResourceManager* m_pResourceManager;
};
