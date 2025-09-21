#include <fstream>
#include <sstream>
#include <string>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

#include <spdlog/spdlog.h>
#include "ResourceManager.hpp"

bool ResourceManager::LoadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		SPDLOG_ERROR("Could not load geometry!");
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
			// Get x, y, (z), r, g, b
			for (int i = 0; i < dimensions + 3; ++i) {
				iss >> value;
				pointData.push_back(value);
			}
		} else if (currentSection == Section::Indices) {
			std::istringstream iss(line);
			// Get corners 0, 1, 2
			for (int i = 0; i < 3; ++i) {
				iss >> value;
				indexData.push_back(value);
			}
		}
	}

	return true;
}

bool ResourceManager::LoadGeometryFromObj(const std::filesystem::path& path, std::vector<VertexAttributes>& vertexData)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());
	if (!warn.empty()) {
		SPDLOG_WARN("{}", warn);
	}
	if (!err.empty()) {
		SPDLOG_ERROR("{}", err);
	}
	if (!ret) {
		return false;
	}

	// Filling in vertexData:
	vertexData.clear();
	for (const auto& shape : shapes) {
		size_t offset = vertexData.size();
		vertexData.resize(offset + shape.mesh.indices.size());

		for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
			const tinyobj::index_t& idx = shape.mesh.indices[i];

			// Avoid mirroring by adding a minus
			vertexData[offset + i].position = {
				attrib.vertices[3 * idx.vertex_index + 0],
				-attrib.vertices[3 * idx.vertex_index + 2], 
				attrib.vertices[3 * idx.vertex_index + 1]
			};

			// Also apply the transform to normals!!
			vertexData[offset + i].normal = {
				attrib.normals[3 * idx.normal_index + 0],
				-attrib.normals[3 * idx.normal_index + 2],
				attrib.normals[3 * idx.normal_index + 1]
			};

			vertexData[offset + i].color = {
				attrib.colors[3 * idx.vertex_index + 0],
				attrib.colors[3 * idx.vertex_index + 1],
				attrib.colors[3 * idx.vertex_index + 2]
			};
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