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

namespace Fish {

	namespace JSON {

		class Handler {
		public:

			Handler() = delete; // Force handler to only be constructed with a filepath.
			Handler(const std::filesystem::path& filepath) : m_Filepath(filepath) {}

			void test_save();

			bool file_exists();

			bool create_file();

			nlohmann::json& format_transform(const Transform& transform, const std::string& name);

		private:
			const std::filesystem::path m_Filepath;
		};
	}
}