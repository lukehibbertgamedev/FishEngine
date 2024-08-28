#include "loader.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vk_engine.h>
#include <vk_initializers.h>
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

namespace Fish {

	namespace Loader {

	}
}
//		bool Fish::Loader::load_image_from_file(FishVulkanEngine& engine, const char* file, AllocatedImage& outImage)
//		{
//			int texWidth, texHeight, texChannels;
//
//			// load a texture directly from file into a CPU array of pixels.
//			stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
//
//			if (!pixels) {
//				std::cout << "Failed to load texture file " << file << std::endl;
//				return false;
//			}
//
//			void* pixel_ptr = pixels;
//			VkDeviceSize imageSize = texWidth * texHeight * 4;
//
//			//the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
//			VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;
//
//			//allocate temporary buffer for holding texture data to upload
//			AllocatedBuffer11 stagingBuffer = engine.create_buffer11(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
//
//			//copy data to buffer
//			void* data;
//			vmaMapMemory(engine.GetAllocator(), stagingBuffer.allocation, &data);
//
//			memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
//
//			vmaUnmapMemory(engine.GetAllocator(), stagingBuffer.allocation);
//
//			//we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
//			stbi_image_free(pixels);
//
//			VkExtent3D imageExtent;
//			imageExtent.width = static_cast<uint32_t>(texWidth);
//			imageExtent.height = static_cast<uint32_t>(texHeight);
//			imageExtent.depth = 1;
//
//			VkImageCreateInfo dimg_info = vkinit::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);
//
//			AllocatedImage newImage;
//
//			VmaAllocationCreateInfo dimg_allocinfo = {};
//			dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//
//			//allocate and create the image
//			vmaCreateImage(engine.GetAllocator(), &dimg_info, &dimg_allocinfo, &newImage.image, &newImage.allocation, nullptr);
//
//			//layout transition so that the driver puts the texture into Linear layout
//			engine.immediate_submit11([&](VkCommandBuffer cmd) {
//				VkImageSubresourceRange range;
//				range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//				range.baseMipLevel = 0;
//				range.levelCount = 1;
//				range.baseArrayLayer = 0;
//				range.layerCount = 1;
//
//				VkImageMemoryBarrier imageBarrier_toTransfer = {};
//				imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//
//				imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//				imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//				imageBarrier_toTransfer.image = newImage.image;
//				imageBarrier_toTransfer.subresourceRange = range;
//
//				imageBarrier_toTransfer.srcAccessMask = 0;
//				imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//
//				//barrier the image into the transfer-receive layout
//				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer); 
//
//				VkBufferImageCopy copyRegion = {};
//				copyRegion.bufferOffset = 0;
//				copyRegion.bufferRowLength = 0;
//				copyRegion.bufferImageHeight = 0;
//
//				copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//				copyRegion.imageSubresource.mipLevel = 0;
//				copyRegion.imageSubresource.baseArrayLayer = 0;
//				copyRegion.imageSubresource.layerCount = 1;
//				copyRegion.imageExtent = imageExtent;
//
//				//copy the buffer into the image
//				vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
//
//				VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;
//
//				imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//				imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//				imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//				imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//
//				//barrier the image into the shader readable layout
//				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);
//			});
//
//			engine.GetDeletionQueue().push_function([=]() { vmaDestroyImage(engine.Get().GetAllocator(), newImage.image, newImage.allocation); });
//
//			vmaDestroyBuffer(engine.GetAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);
//
//			std::cout << "Texture loaded successfully " << file << std::endl;
//
//			outImage = newImage;
//			return true;
//		}
//
//	}
//}

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(FishVulkanEngine* engine, std::filesystem::path filePath)
{
	std::cout << "Loading GLTF: " << filePath << std::endl;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filePath);

	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers
		| fastgltf::Options::LoadExternalBuffers;

	fastgltf::Asset gltf;
	fastgltf::Parser parser{};

	auto load = parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions);
	if (load) {
		gltf = std::move(load.get());
	}
	else {
		fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
		return {};
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;

	// use the same vectors for all meshes so that the memory doesnt reallocate as
	// often
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		MeshAsset newmesh;

		newmesh.name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) {
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

			size_t initial_vtx = vertices.size();

			// load indexes
			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
					[&](std::uint32_t idx) {
						indices.push_back(idx + initial_vtx);
					});
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index) {
						Vertex newvtx;
				newvtx.position = v;
				newvtx.normal = { 1, 0, 0 };
				newvtx.color = glm::vec4{ 1.f };
				newvtx.uv_x = 0;
				newvtx.uv_y = 0;
				vertices[initial_vtx + index] = newvtx;
					});
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
					[&](glm::vec3 v, size_t index) {
						vertices[initial_vtx + index].normal = v;
					});
			}

			// load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
					[&](glm::vec2 v, size_t index) {
						vertices[initial_vtx + index].uv_x = v.x;
				vertices[initial_vtx + index].uv_y = v.y;
					});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
					[&](glm::vec4 v, size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}
			newmesh.surfaces.push_back(newSurface);
		}

		// display the vertex normals
		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}
		newmesh.meshBuffers = engine->uploadMesh(indices, vertices);
		//newmesh.meshBuffers = FishVulkanEngine::Get().uploadMesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
	}

	return meshes;
}
