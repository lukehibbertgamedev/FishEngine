#include "fish_json_handler.h"

#include <fmt/format.h>

void Fish::JSON::Handler::save(std::unordered_map<std::string, std::shared_ptr<Fish::Loader::LoadedGLTF>> loadedScenes, Fish::Camera camera)
{
	// Make sure we have a file to write to.
	if (!file_exists()) {
		create_file();
	}

	// Load all json objects into here to be written to json.
	std::vector<nlohmann::json> dataToSave = {};

	// Format our camera data into a save-able state.
	nlohmann::json cameraData = nlohmann::json::object();
	cameraData["name"] = { "mainCamera" };
	cameraData["position"] = { camera.m_Position.x, camera.m_Position.y, camera.m_Position.z };
	cameraData["pitch"] = { camera.m_Pitch };
	cameraData["yaw"] = { camera.m_Yaw };
	dataToSave.push_back(cameraData);

	// Format all object save-able data into one container.
	for (auto& data : loadedScenes)
	{
		const std::string& name = data.first;
		Fish::Loader::LoadedGLTF& obj = *data.second;
		Transform thisTransform = obj.transform;

		nlohmann::json jsonData = nlohmann::json::object();
		jsonData["name"] = { name };
		jsonData["position"] = { thisTransform.position.x, thisTransform.position.y, thisTransform.position.z };
		jsonData["rotation"] = { thisTransform.rotation.x, thisTransform.rotation.y, thisTransform.rotation.z };
		jsonData["scale"] = { thisTransform.scale.x, thisTransform.scale.y, thisTransform.scale.z };

		dataToSave.push_back(jsonData);
	}

	// Write all the data from our container.
	std::ofstream outputFile(m_Filepath);
	if (outputFile.is_open()) {
		for (int i = 0; i < dataToSave.size(); ++i) {
			outputFile << std::setw(4) << dataToSave[i] << std::endl;
			FISH_LOG("- Data has been written to JSON.");
		}
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

//nlohmann::json& Fish::JSON::Handler::format_transform(const Transform& transform, const std::string& name)
//{
//	nlohmann::json data = nlohmann::json::object();
//	data["name"] = { name };
//	data["position"] = { transform.position.x, transform.position.y, transform.position.z };
//	data["rotation"] = { transform.rotation.x, transform.rotation.y, transform.rotation.z };
//	data["scale"] = { transform.scale.x, transform.scale.y, transform.scale.z };
//	return data;
//}
