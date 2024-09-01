
#pragma once
#include <vk_types.h>
#include <vk_descriptors.h>

#include <unordered_map>
#include <filesystem>

//forward declaration
class FishVulkanEngine;

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct GLTFMaterial {
    MaterialInstance data;
};

// Contains sub-meshes of the main mesh.
struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

// Entire mesh asset containing a name and the mesh buffers with an array of 
// GeoSurfaces for the sub-meshes that this full mesh will contain.
struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

// See (https://vkguide.dev/docs/new_chapter_5/gltf_nodes/) for more.
struct LoadedGLTF : public IRenderable {

    // storage for all the data on a given gltf file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer13 materialDataBuffer;

    FishVulkanEngine* creator;

    ~LoadedGLTF() { clearAll(); };

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        // create renderables from the scenenodes
        for (auto& n : topNodes) {
            n->Draw(topMatrix, ctx);
        }
    }

private:

    void clearAll();
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(FishVulkanEngine* engine, std::filesystem::path filePath);

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(FishVulkanEngine* engine, std::string_view filePath);

namespace Fish {

	namespace Loader {

		//bool load_image_from_file(FishVulkanEngine& engine, const char* file, AllocatedImage& outImage);        
	}
}