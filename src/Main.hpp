#pragma once

#include <utility>

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

	bool m_gpuIdle = false;

	std::pair<WGPUSurfaceTexture, WGPUTextureView> getNextSurfaceViewData();
	WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options);
	WGPUDevice requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);
	void initializePipeline();
};