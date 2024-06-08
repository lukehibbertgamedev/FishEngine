
// This will contain image related vulkan helpers.

#pragma once 

namespace vkutil {

	// We should transition the swapchain image to a drawable layout, perform VkCmdClear, and transition it back.
	// We need a way to transition images as part of a command buffer instead of using a renderpass.
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);
};