#include <fstream>
#include <sstream>
#include <string>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

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
				attrib.vertices[3 * idx.vertex_index],
				-attrib.vertices[3 * idx.vertex_index + 2], 
				attrib.vertices[3 * idx.vertex_index + 1]
			};

			// Also apply the transform to normals!!
			vertexData[offset + i].normal = {
				attrib.normals[3 * idx.normal_index],
				-attrib.normals[3 * idx.normal_index + 2],
				attrib.normals[3 * idx.normal_index + 1]
			};

			vertexData[offset + i].color = {
				attrib.colors[3 * idx.vertex_index],
				attrib.colors[3 * idx.vertex_index + 1],
				attrib.colors[3 * idx.vertex_index + 2]
			};

			vertexData[offset + i].uv = {
				attrib.texcoords[2 * idx.texcoord_index],
				1 - attrib.texcoords[2 * idx.texcoord_index + 1] // Invert V axis for modern graphics APIs (Vulkan, DX12, etc.)
			};
		}
	}

	return true;
}

void ResourceManager::writeMipMaps(WGPUDevice device, WGPUTexture texture, WGPUExtent3D textureSize, uint32_t mipLevelCount, const unsigned char* pixelData)
{
	WGPUImageCopyTexture destination;
	destination.texture = texture;
	destination.mipLevel = 0;
	destination.origin = {0, 0, 0}; // Offset for writeBuffer (what part of the image do we change)
	destination.aspect = WGPUTextureAspect_All; // Only relevant to depth/stencil textures

	WGPUTextureDataLayout source;
	source.nextInChain = nullptr;
	source.offset = 0;
	source.bytesPerRow = 4 * textureSize.width;
	source.rowsPerImage = textureSize.height;

	WGPUQueue queue = wgpuDeviceGetQueue(device);
	wgpuQueueWriteTexture(queue, &destination, pixelData, 4 * textureSize.width * textureSize.height, &source, &textureSize);
	wgpuQueueRelease(queue);
}

WGPUTexture ResourceManager::LoadTexture(const std::filesystem::path& path, WGPUDevice device, WGPUTextureView* pTextureView)
{
	int width, height, channels;
	std::string pathName = path.string();
	unsigned char* pixelData = stbi_load(pathName.c_str(), &width, &height, &channels, 4); // Force 4 channels
	if (pixelData == nullptr) {
		SPDLOG_ERROR("Failed to load texture \"{}\"", pathName);
		exit(1);
	}

	WGPUTextureDescriptor textureDesc = {};
	textureDesc.nextInChain = nullptr;
	textureDesc.label = "My loaded texture";
	textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	textureDesc.dimension = WGPUTextureDimension_2D;
	textureDesc.size = {(unsigned int)width, (unsigned int)height, 1};
	textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
	textureDesc.mipLevelCount = 1; // 8 mip maps
	textureDesc.sampleCount = 1;
	textureDesc.viewFormatCount = 0;
	textureDesc.viewFormats = nullptr;
	WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDesc);
	
	// Upload data to the GPU texture
	writeMipMaps(device, texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

	stbi_image_free(pixelData);

	// Write to the texture view of the sampler
	if (pTextureView) {
		WGPUTextureViewDescriptor textureViewDesc = {};
		textureViewDesc.nextInChain = nullptr;
		textureViewDesc.label = "My generated texture view";
		textureViewDesc.format = textureDesc.format;
		textureViewDesc.dimension = WGPUTextureViewDimension_2D;
		textureViewDesc.baseMipLevel = 0;
		textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
		textureViewDesc.baseArrayLayer = 0;
		textureViewDesc.arrayLayerCount = 1;
		textureViewDesc.aspect = WGPUTextureAspect_All;
		*pTextureView = wgpuTextureCreateView(texture, &textureViewDesc);
	}

	return texture;
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