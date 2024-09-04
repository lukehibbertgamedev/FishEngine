#pragma once

// name: fish_json_handler.h
// desc: A class for our JSON abstractions using nlohmann.
// auth: Luke Hibbert

#include <filesystem>
#include <fstream>

#include <fish_logger.h>
#include <vk_types.h>

#include <nlohmann_json.h>
#include <nlohmann_json_fwd.h>
#include <fish_loader.h>

namespace Fish {

	namespace JSON {

		class Handler {
		public:

			Handler() = delete; // Force handler to only be constructed with a filepath.
			Handler(const Fish::JSON::Handler&) = delete; // Force handler to only be constructed with a filepath.
			Handler(const std::filesystem::path& filepath, std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> loadedScenes) : m_Filepath(filepath), m_LoadedScenes(loadedScenes) {}

			void save();

			bool file_exists();

			bool create_file();

		private:
			const std::filesystem::path m_Filepath;
			std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> m_LoadedScenes;
		};
	}
}