#pragma once

// name: fish_json_handler.h
// desc: A class for our JSON abstractions using nlohmann.
// auth: Luke Hibbert

#include <filesystem>
#include <fstream>
#include <vector>

#include <fish_logger.h>
#include <vk_types.h>

#include <nlohmann_json.h>
#include <nlohmann_json_fwd.h>
#include <fish_loader.h>
#include <fish_camera.h>
#include <fish_resource_data.h>


namespace Fish {

	namespace JSON {

		class Handler {
		public:
			Handler() = delete; // Force handler to only be constructed with a filepath.
			Handler(const Fish::JSON::Handler&) = delete; // Force handler to only be constructed with a filepath.
			Handler(const std::filesystem::path& filepath) : m_Filepath(filepath) {}

			glm::vec3 parse_vec3(const nlohmann::json& vec);
			nlohmann::json serialise_vec3(const glm::vec3& vec);

			bool file_exists();
			bool create_file();

			void serialise_scene_data(std::string sceneName, std::unordered_map<std::string, Fish::ResourceData::Object> objectsInScene, Fish::Camera camera);
			void parse_scene_data(std::string& outSceneName, std::unordered_map<std::string, Fish::ResourceData::Object>& outObjectData, Fish::ResourceData::Camera& outCameraData);

		private:
			const std::filesystem::path m_Filepath;
		};
	}
}