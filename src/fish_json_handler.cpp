#include "fish_json_handler.h"

#include <fmt/format.h>
#include <iostream>

glm::vec3 Fish::JSON::Handler::parse_vec3(const nlohmann::json& vec)
{
	return glm::vec3(vec.at(0).get<float>(), vec.at(1).get<float>(), vec.at(2).get<float>());
}

nlohmann::json Fish::JSON::Handler::serialise_vec3(const glm::vec3& vec)
{
	return nlohmann::json::array({ vec.x, vec.y, vec.z });
}

void Fish::JSON::Handler::save(std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> loadedScenes, Fish::Camera camera)
{
	// Make sure we have a file to write to.
	if (!file_exists()) {
		create_file();
	}

	// Our top level container to hold the camera and object data.
	nlohmann::json jsonData = nlohmann::json::object();

	// Format our camera data into a save-able state.
	nlohmann::json cameraData = nlohmann::json::object();
	cameraData["position"] = { camera.m_Position.x, camera.m_Position.y, camera.m_Position.z };
	cameraData["pitch"] = { camera.m_Pitch };
	cameraData["yaw"] = { camera.m_Yaw };
	jsonData["mainCamera"] = cameraData; // Add this to the top level.

	// Format all object save-able data into one container.
	for (const auto& [key, data] : loadedScenes)
	{
		nlohmann::json objectData = nlohmann::json::object();
		objectData["position"] = serialise_vec3(data->transform.position);
		objectData["rotation"] = serialise_vec3(data->transform.rotation);
		objectData["scale"] = serialise_vec3(data->transform.scale);
		jsonData[key] = objectData; // Todo: Add this to the object level.
	}

	// Write all the data from our container.
	std::ofstream outputFile(m_Filepath);
	if (outputFile.is_open()) {
		outputFile << std::setw(4) << jsonData << std::endl;
		FISH_LOG("- Data has been written to JSON.");
	}
	else {
		FISH_FATAL("Failed to open for writing.");
	}
}

void Fish::JSON::Handler::load_camera_data(std::vector<Fish::ResourceData::Camera>& outCameraData)
{
	
}

void Fish::JSON::Handler::load_object_data(std::vector<Fish::ResourceData::Object>& outObjectData)
{
	std::vector<Fish::ResourceData::Object> dataContainer = {};

	std::ifstream input(m_Filepath);
	if (!input.is_open()) {
		FISH_FATAL("Failed to open file for reading.");
	}

	nlohmann::json data;
	input >> data;
	input.close();

	for (const auto& [key, value] : data.items()) {

		Fish::ResourceData::Object object;
		object.name = key;
		object.position = parse_vec3(value.at("position"));
		object.rotation = parse_vec3(value.at("rotation"));
		object.scale = parse_vec3(value.at("scale"));
		dataContainer.push_back(object);
	}

	for (const auto& obj : dataContainer) {
		std::cout << "Object: " << obj.name << std::endl;
		std::cout << "  Position: (" << obj.position.x << ", " << obj.position.y << ", " << obj.position.z << ")" << std::endl;
		std::cout << "  Rotation: (" << obj.rotation.x << ", " << obj.rotation.y << ", " << obj.rotation.z << ")" << std::endl;
		std::cout << "  Scale: (" << obj.scale.x << ", " << obj.scale.y << ", " << obj.scale.z << ")" << std::endl;
	}

	outObjectData = dataContainer;
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

//nlohmann::json& Fish::JSON::Handler::format_transform(const Transform& transform, const std::string& name)
//{
//	nlohmann::json data = nlohmann::json::object();
//	data["name"] = { name };
//	data["position"] = { transform.position.x, transform.position.y, transform.position.z };
//	data["rotation"] = { transform.rotation.x, transform.rotation.y, transform.rotation.z };
//	data["scale"] = { transform.scale.x, transform.scale.y, transform.scale.z };
//	return data;
//}
