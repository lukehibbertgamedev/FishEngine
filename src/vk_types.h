// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

// The entire codebase will include this header. it will provide widely used default structures and includes.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

constexpr bool kUseValidationLayers = true;

constexpr unsigned int kFrameOverlap = 2;

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer13 {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

// The reason that the UV parameters are interleaved (weirdly positioned) is because of alignment limitations
// on GPUs, it likes to be within blocks of 16 and v3 + float basically = v4 which is beautiful alignment.
struct Vertex {

    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// Holds the resources needed for a mesh
struct GPUMeshBuffers {

    AllocatedBuffer13 indexBuffer;
    AllocatedBuffer13 vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// Push constants for our mesh object draw calls
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};
struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

// Macro to ensure no errors occur. Used in functions that return a VkResult.
// This will also fail on the actual line of error making it easier to debug.
#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)