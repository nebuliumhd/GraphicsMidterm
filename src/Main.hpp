#pragma once

#include <array>
#include <utility>
#include <filesystem>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#include <glm/glm.hpp>

struct MyUniforms // Total size of the struct has to be a multiple of the alignment size of its largest field
{
	glm::mat4x4 projectionMatrix;
	glm::mat4x4 viewMatrix;
	glm::mat4x4 modelMatrix;
	glm::vec4 color;
	float time;
	float _pad[3];
};
static_assert(sizeof(MyUniforms) % 16 == 0);

struct VertexAttributes
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;
};

class Application
{
public:
	bool Initialize();
	void Terminate();
	void MainLoop();
	bool IsRunning();
private:
	GLFWwindow* m_glfwWindow = nullptr;
	WGPUInstance m_instance = nullptr;
	WGPUAdapter m_adapter = nullptr;
	WGPUDevice m_device = nullptr;
	WGPUQueue m_queue = nullptr;
	WGPUSurface m_surface = nullptr;
	WGPURenderPipeline m_pipeline = nullptr;
	WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_Undefined;
	WGPUBuffer m_vertexBuffer = nullptr, m_indexBuffer = nullptr, m_uniformBuffer = nullptr;
	WGPUPipelineLayout m_layout = nullptr;
	WGPUBindGroup m_bindGroup = nullptr;
	WGPUBindGroupLayout m_bindGroupLayout = nullptr;
	WGPUTexture m_texture = nullptr, m_depthTexture = nullptr;
	WGPUTextureView m_textureView = nullptr, m_depthTextureView = nullptr;
	WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Undefined;
	WGPUSampler m_sampler = nullptr;

	bool m_gpuIdle = false;
	// uint32_t m_vertexCount = 0;
	std::vector<VertexAttributes> m_vertexData;
	uint32_t m_indexCount = 0;
	MyUniforms m_uniforms;
	uint32_t m_uniformStride = 0;

	uint32_t ceilToNextMultiple(uint32_t value, uint32_t step);
	std::pair<WGPUSurfaceTexture, WGPUTextureView> getNextSurfaceViewData();
	WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options);
	WGPUDevice requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);
	WGPURequiredLimits getRequiredLimits(WGPUAdapter adapter) const;
	void setDefault(WGPULimits& limits) const;
	void setDefault(WGPUBindGroupLayoutEntry& bindingLayout) const;
	void setDefault(WGPUStencilFaceState& stencilFaceState) const;
	void setDefault(WGPUDepthStencilState& depthStencilState) const;
	void displayAdapterInfo(WGPUAdapter adapter);
	void inspectDevice(WGPUDevice device);
	
	void initializeBuffers();
	void initializePipeline();
	void initializeBindGroups();
	void calculateUniforms();
};