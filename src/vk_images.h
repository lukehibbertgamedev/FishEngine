#pragma once 

// name: vk_images.h
// desc: Helper functions for use with images.
// desc: Most of the code is modified from the Vulkan Guide by vblanco20-1 on Git.
// auth: Luke Hibbert

namespace vkutil {

	// We should transition the swapchain image to a drawable layout, perform VkCmdClear, and transition it back.
	// We need a way to transition images as part of a command buffer instead of using a renderpass.
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	// Abstraction to create a VkImageSubresourceRange structure.
	VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

	// Set up blitting information structures and calls vkCmdBlitImage2 to copy regions of a source image into a destination image.
	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

	// Calculate the number of levels in the mip chain, and generate mipmaps in a chain of vkCmdBlitImage2 calls.
	// For each mip level, we copy the image from the level before it and lower the resolution by half each time.
	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
};