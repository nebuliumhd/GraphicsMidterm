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

struct LightingUniforms
{
	std::array<glm::vec4, 2> directions;
	std::array<glm::vec4, 2> colors;
};
static_assert(sizeof(LightingUniforms) % 16 == 0);

struct VertexAttributes
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;
};

struct CameraState
{
	// Rotation around the global vertical axis and local horizontal axis respectively (xmouse, ymouse)
	glm::vec2 angles = {0.8f, 0.5f};
	// Position of camera along its local forward axis (scroll wheel)
	float zoom = -1.2f;
};

struct DragState
{
	bool active = false; // On-going drag
	glm::vec2 startMouse; // Mouse at the start of the drag
	CameraState startCameraState; // Camera at the start of the drag

	float sensitivity = 0.01f;
	float scrollSensitivity = 0.1f;

	glm::vec2 velocity = {0.0f, 0.0f};
	glm::vec2 previousDelta;
	float inertia = 0.9f;
};

class Application
{
public:
	bool Initialize();
	void Terminate();
	void MainLoop();
	bool IsRunning();

	void onResize();
private:
	GLFWwindow* m_glfwWindow = nullptr;
	WGPUInstance m_instance = nullptr;
	WGPUAdapter m_adapter = nullptr;
	WGPUDevice m_device = nullptr;
	WGPUQueue m_queue = nullptr;
	WGPUSurface m_surface = nullptr;
	WGPUSwapChain m_swapChain = nullptr;
	WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
	WGPURenderPipeline m_pipeline = nullptr;
	WGPUBuffer m_vertexBuffer = nullptr, m_indexBuffer = nullptr, m_uniformBuffer = nullptr, m_lightingUniformBuffer = nullptr;
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
	uint32_t m_vertexCount = 0;
	MyUniforms m_uniforms;
	LightingUniforms m_lightingUniforms;
	bool m_lightingUniformsChanged = true;
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
	
	// For the initialization of the class
	bool initWindowAndDevice();
	bool initSwapChain();
	bool initDepthBuffer();
	bool initTexture();
	bool initGeometry();
	bool initUniforms();
	bool initLightingUniforms();
	bool initBindGroupLayout();
	bool initRenderPipeline();
	bool initBindGroup();

	void updateProjectionMatrix();
	void updateViewMatrix();
	void updateLightingUniforms();

	// Input
	CameraState m_cameraState;
	DragState m_dragState;
	void updateDragInertia();
	void onMouseMove(double xpos, double ypos);
	void onMouseButton(int button, int action, int mods);
	void onScroll(double xoffset, double yoffset);

	// Dear ImGui
	bool initDearImGui();
	void updateDearImGui(WGPURenderPassEncoder renderPassEncoder);
};