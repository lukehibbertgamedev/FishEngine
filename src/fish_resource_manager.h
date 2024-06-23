
#pragma once

// Singleton class with thanks to https://gameprogrammingpatterns.com/singleton.html

#include <fish_resource.h>
#include <fish_scene.h>

#include <unordered_map>

namespace Fish {

	class ResourceManager {
	public:
		
		void init();

		void load_all_meshes();
		void load_all_textures();

		// todo expand this to be load all scenes which reads from a save file.
		void load_scene();

		Fish::Resource::Mesh create_default_triangle();

		// todo (required indices, fourth vertex not rendered)
		Fish::Resource::Mesh create_default_quad();

		Fish::Resource::Mesh* get_mesh_by_name(const std::string& name);

		Fish::Resource::Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
		Fish::Resource::Material* get_material_by_name(const std::string& name); //search for the object, and return nullptr if not found

		Fish::Scene& get_current_scene() { return m_Scene; }

		// singleton implementation.
		static ResourceManager& Get() { static ResourceManager instance; return instance; }
		ResourceManager(ResourceManager const&) = delete;
		void operator=(ResourceManager const&) = delete;
	private: 
		ResourceManager() {} // private constructor for singleton implementation

		void upload_mesh(Fish::Resource::Mesh& mesh);

		Fish::Scene m_Scene;

		std::unordered_map<std::string, Fish::Resource::Mesh> m_Meshes;
		std::unordered_map<std::string, Fish::Resource::Texture> m_Textures;
		std::unordered_map<std::string, Fish::Resource::Material> m_Materials;
	};
}
