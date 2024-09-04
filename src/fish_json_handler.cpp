#include "fish_json_handler.h"

#include <fmt/format.h>

void Fish::JSON::Handler::test_save()
{
	if (!file_exists()) {
		create_file();
	}

	Transform t;
	t.position.x = 5.0f;
	t.position.y = 10.0f;
	t.position.z = 15.0f;
	t.rotation.x = 90.0f;
	t.rotation.y = 180.0f;
	t.rotation.z = 45.0f;
	t.scale.x = 1.0f;
	t.scale.y = 1.5f;
	t.scale.z = 2.0f;
	std::string n = "House_Test";

	nlohmann::json data = nlohmann::json::object();
	data["name"] = { "House_Test" };
	data["position"] = { 0.0f, 5.0f, 10.0f };
	data["rotation"] = { 90.0f, 45.0f, 180.0f };
	data["scale"] = { 1.0f, 1.5f, 2.0f };

	std::ofstream outputFile(m_Filepath);
	if (outputFile.is_open()) {
		outputFile << std::setw(4) << data << std::endl;
		FISH_LOG("Test data has been written to JSON.");
	}
	else {
		FISH_FATAL("Failed to open for writing.");
	}
}

bool Fish::JSON::Handler::file_exists()
{
	return (std::filesystem::exists(m_Filepath) ? true : false);
}

bool Fish::JSON::Handler::create_file()
{
	nlohmann::json data = nlohmann::json::object();
	std::ofstream output(m_Filepath);
	if (output.is_open()) {
		output << std::setw(4) << data << std::endl;
		FISH_LOG("Empty JSON file created.");
		return true;
	}
	else {
		FISH_FATAL("Failed to create JSON file.");
		assert(false);
		return false;
	}
}

nlohmann::json& Fish::JSON::Handler::format_transform(const Transform& transform, const std::string& name)
{
	nlohmann::json data = nlohmann::json::object();
	data["name"] = { name };
	data["position"] = { transform.position.x, transform.position.y, transform.position.z };
	data["rotation"] = { transform.rotation.x, transform.rotation.y, transform.rotation.z };
	data["scale"] = { transform.scale.x, transform.scale.y, transform.scale.z };
	return data;
}
