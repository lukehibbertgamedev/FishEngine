
#pragma once
#include <vk_types.h>

#include <unordered_map>
#include <filesystem>

//forward declaration
class FishVulkanEngine;

// Contains sub-meshes of the main mesh.
struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

// Entire mesh asset containing a name and the mesh buffers with an array of 
// GeoSurfaces for the sub-meshes that this full mesh will contain.
struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(FishVulkanEngine* engine, std::filesystem::path filePath);

namespace Fish {

	namespace Loader {

		//bool load_image_from_file(FishVulkanEngine& engine, const char* file, AllocatedImage& outImage);        
	}
}