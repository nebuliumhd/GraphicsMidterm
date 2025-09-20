#include <fstream>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>
#include "ResourceManager.hpp"

bool ResourceManager::LoadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	pointData.clear();
	indexData.clear();

	enum class Section
	{
		None,
		Points,
		Indices
	};
	Section currentSection = Section::None;

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof()) {
		std::getline(file, line);

		// Overcome CRLF problem
		if (!line.empty() && line.back() == '\r') {
			line.pop_back(); // Removes last character
		}

		if (line == "[points]") {
			currentSection = Section::Points;
		} else if (line == "[indices]") {
			currentSection = Section::Indices;
		} else if (line[0] == '#' || line.empty()) {
			// Do nothing yet
		} else if (currentSection == Section::Points) {
			std::istringstream iss(line);
			// Get x, y, r, g, b
			for (int i = 0; i < 5; ++i) {
				iss >> value;
				pointData.push_back(value);
			}
		} else if (currentSection == Section::Indices) {
			std::istringstream iss(line);
			// Get x, y, r, g, b
			for (int i = 0; i < 3; ++i) {
				iss >> value;
				indexData.push_back(value);
			}
		}
	}

	return true;
}

WGPUShaderModule ResourceManager::LoadShaderModule(const std::filesystem::path& path, WGPUDevice device)
{
	SPDLOG_INFO("Loading shader module...");
	std::ifstream file(path);
	if (!file.is_open()) {
		return nullptr;
	}
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string shaderSource(size, ' ');
	file.seekg(0);
	file.read(shaderSource.data(), size);

	WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	shaderCodeDesc.code = shaderSource.c_str();

	WGPUShaderModuleDescriptor shaderDesc = {};
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	shaderDesc.label = "Main shader module";

	return wgpuDeviceCreateShaderModule(device, &shaderDesc);
}