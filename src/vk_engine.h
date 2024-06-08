// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

// This will be the main class for the engine, and where most of the code of the vulkan guide tutorial will go.

#pragma once

#include <vk_types.h>

class VulkanEngine {
public:

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
	void create_commands();
	void create_synchronisation_structures();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

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
