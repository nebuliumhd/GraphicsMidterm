#pragma once

#include <vector>
#include <filesystem>

#include <webgpu/webgpu.h>

#include "Main.hpp"

class ResourceManager
{
public:
	static bool LoadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions);
	static bool LoadGeometryFromObj(const std::filesystem::path& path, std::vector<VertexAttributes>& vertexData);
	static WGPUTexture LoadTexture(const std::filesystem::path& path, WGPUDevice device, WGPUTextureView* pTextureView = nullptr);
	static WGPUShaderModule LoadShaderModule(const std::filesystem::path& path, WGPUDevice device);
private:
	static void writeMipMaps(WGPUDevice device, WGPUTexture texture, WGPUExtent3D textureSize, uint32_t mipLevelCount, const unsigned char* pixelData);
};