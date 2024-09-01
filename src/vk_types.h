#pragma once

// name: vk_types.h
// desc: A collection of important header files and structures that most of the codebase will include.
// auth: Luke Hibbert

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

// Flag to enable/disable the use of validation layers. Should be put into a constants/util file.
constexpr bool kUseValidationLayers = true;

// 2 for double buffering. Should be put into a constants/util file.
constexpr unsigned int kFrameOverlap = 2;

// ...
struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

// 1.3 - ...
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

// Holds the resources needed for a mesh.
struct GPUMeshBuffers {

    AllocatedBuffer13 indexBuffer;
    AllocatedBuffer13 vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// Push constants for our mesh object draw calls.
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

// ...
enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};

// ...
struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

// ...
struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

// Forward declaration.
struct DrawContext;

// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        // draw children
        for (auto& c : children) {
            c->Draw(topMatrix, ctx);
        }
    }
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