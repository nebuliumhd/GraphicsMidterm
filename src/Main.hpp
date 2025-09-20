#pragma once

#include <utility>
#include <filesystem>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

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
	WGPUBuffer m_buffer1 = nullptr, m_buffer2 = nullptr;
	WGPUBuffer m_pointBuffer = nullptr, m_indexBuffer = nullptr;
	WGPUBuffer m_uniformBuffer = nullptr;
	WGPUPipelineLayout m_layout = nullptr;
	WGPUBindGroup m_bindGroup = nullptr;
	WGPUBindGroupLayout m_bindGroupLayout = nullptr;

	bool m_gpuIdle = false;
	// uint32_t m_vertexCount = 0;
	uint32_t m_indexCount = 0;

	std::pair<WGPUSurfaceTexture, WGPUTextureView> getNextSurfaceViewData();
	WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options);
	WGPUDevice requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);
	WGPURequiredLimits getRequiredLimits(WGPUAdapter adapter) const;
	void setDefault(WGPULimits& limits) const;
	void setDefault(WGPUBindGroupLayoutEntry& bindingLayout) const;
	void initializePipeline();
	void foobarBuffers();
	void intializeBuffers();
	void initializeBindGroups();
};