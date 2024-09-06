#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <fish_loader.h>

// name: fish_resource_manager.h
// desc: A system to manage the loading of all resources in our engine.
// auth: Luke Hibbert

// !! Currently not in use due to the refactor, this will be returning.


class ResourceManager {
public:
	std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> loadedResources;

	// singleton implementation.
	static ResourceManager& Get() { static ResourceManager instance; return instance; }
	ResourceManager(ResourceManager const&) = delete;
	void operator=(ResourceManager const&) = delete;
private: 
	ResourceManager() {} // private constructor for singleton implementation
};

//
//#pragma once
//
//// Singleton class with thanks to https://gameprogrammingpatterns.com/singleton.html
//
//#include <fish_resource.h>
//#include <fish_scene.h>
//
//#include <unordered_map>
//
//namespace Fish {
//
//	class ResourceManager {
//	public:
//		
//		void init();
//
//		void load_all_meshes();
//		void load_all_textures();
//
//		// todo expand this to be load all scenes which reads from a save file.
//		void load_scene();
//
//		Fish::Resource::Mesh create_default_triangle();
//		Fish::Resource::Mesh create_default_quad();
//
//		Fish::Resource::Mesh create_default_cube();
//
//		Fish::Resource::Mesh* get_mesh_by_name(const std::string& name);
//
//		Fish::Resource::Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
//		Fish::Resource::Material* get_material_by_name(const std::string& name); //search for the object, and return nullptr if not found
//
//		Fish::Scene& get_current_scene() { return m_Scene; }
//		Fish::Scene m_Scene;
//
//		//void sample_texture(const std::string& name, VkSamplerCreateInfo samplerInfo, VkSampler blockySampler, VkDescriptorSetAllocateInfo allocInfo, VkWriteDescriptorSet texture1);
//
//		// 1.3 - Create structure for push constants required to draw a mesh.
//		// Span is basically a pointer and a size pair.
//		// See (https://vkguide.dev/docs/new_chapter_3/mesh_buffers/) for more.
//		//GPUMeshBuffers upload_mesh13(std::span<uint32_t> indices, std::span<Vertex> vertices);
//
//		// singleton implementation.
//		static ResourceManager& Get() { static ResourceManager instance; return instance; }
//		ResourceManager(ResourceManager const&) = delete;
//		void operator=(ResourceManager const&) = delete;
//	private: 
//		ResourceManager() {} // private constructor for singleton implementation
//
//		void upload_mesh11(Fish::Resource::Mesh& mesh);
//
//
//		std::unordered_map<std::string, Fish::Resource::Mesh> m_Meshes;
//		std::unordered_map<std::string, Fish::Resource::Texture> m_Textures;
//		std::unordered_map<std::string, Fish::Resource::Material> m_Materials;
//	};
//}
