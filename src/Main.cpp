#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>
#include <fstream>
#include <array>
#include <thread>

// Z is (0, 1) and not OpenGL's (-1, 1)
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// Left-handed coordinate system
#define GLM_FORCE_LEFT_HANDED
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>
#include <glfw3webgpu.h>
#include "Main.hpp"
#include "ResourceManager.hpp"

#ifndef RESOURCE_DIR
#error "A RESOURCE_DIR must be defined to compile the project!"
#endif

constexpr float PI = 3.14159265358979323846f;

// Takes value and rounds it up to the next multiple of step
uint32_t Application::ceilToNextMultiple(uint32_t value, uint32_t step)
{
	uint32_t divideAndCeil = value / step + (value % step == 0 ? 0 : 1);
	return step * divideAndCeil;
}

WGPUAdapter Application::requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options)
{
	// Use this as a Future<WGPUAdapter> so the asynchronous callback gets the adapter within the struct
	struct UserData
	{
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData)
	{
		UserData& userData = *reinterpret_cast<UserData*>(pUserData);
		if (status == WGPURequestAdapterStatus_Success) {
			userData.adapter = adapter;
		} else {
			SPDLOG_ERROR("Could not get WebGPU adapter: {}", message);
			exit(1);
		}
		userData.requestEnded = true;
	};

	wgpuInstanceRequestAdapter(
		instance,
		options,
		onAdapterRequestEnded, // Synchronous on desktop?
		(void*)&userData
	);

	assert(userData.requestEnded); // Should be done
	return userData.adapter;
}

WGPUDevice Application::requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor)
{
	struct UserData
	{
		WGPUDevice device = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData)
	{
		UserData& userData = *reinterpret_cast<UserData*>(pUserData);
		if (status == WGPURequestDeviceStatus_Success) {
			userData.device = device;
		} else {
			SPDLOG_ERROR("Could not get the WebGPU device: {}");
			exit(1);
		}
		userData.requestEnded = true;
	};

	wgpuAdapterRequestDevice(
		adapter,
		descriptor,
		onDeviceRequestEnded,
		(void*)&userData
	);

	assert(userData.requestEnded); // Should be done
	return userData.device;
}

WGPURequiredLimits Application::getRequiredLimits(WGPUAdapter adapter) const
{
	WGPUSupportedLimits supportedLimits = {};
	supportedLimits.nextInChain = nullptr;
	wgpuAdapterGetLimits(m_adapter, &supportedLimits);

	WGPURequiredLimits requiredLimits = {};
	setDefault(requiredLimits.limits);

	// Vertex buffers

	// Three vertex attributes (position, normal, color, and uv)
	requiredLimits.limits.maxVertexAttributes = 4;
	// One vertex buffer
	requiredLimits.limits.maxVertexBuffers = 1;
	// In otherwords, how many individual attributes can be passed from the vertex to fragment shader (x, y, z), (nx, ny, nz),  and (u, v)
	requiredLimits.limits.maxInterStageShaderComponents = 8;
	// Minimum required buffer size needed (10k vertices allowed for meshes)
	requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	// Maximum stride between consecutive vertices in a vertex buffer
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes); // For X, Y, Z, R, G, B

	// Uniforms

	// For texture uniforms
	requiredLimits.limits.maxBindGroups = 1;
	// Use at most 1 uniform buffer per stage
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	// Uniform structs have a size of a maximum 16 floats (way more than needed)
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	// Extra limit requirement
	requiredLimits.limits.maxDynamicUniformBuffersPerPipelineLayout = 1;

	// Textures / Depth buffer

	requiredLimits.limits.maxSamplersPerShaderStage = 1; // Allows for one sampler in the shader
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 1; // Allows a single texture to be sampled in a shader
	// Upped both for higher-resolution textures
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
	requiredLimits.limits.maxTextureArrayLayers = 1;

	// IMPORTANT!!! MUST SET THESE TO AN INITIALIZED VALUE

	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

	return requiredLimits;
}

void Application::setDefault(WGPULimits& limits) const
{
	limits.maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxBindGroups = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	limits.minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxBufferSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED;
}

void Application::setDefault(WGPUBindGroupLayoutEntry& bindingLayout) const
{
	bindingLayout.nextInChain = nullptr;
	
	// Need to be changed by the function that calls this
	bindingLayout.binding = 0;
	bindingLayout.visibility = WGPUShaderStage_None;

	bindingLayout.buffer.nextInChain = nullptr;
	bindingLayout.buffer.type = WGPUBufferBindingType_Undefined;
	bindingLayout.buffer.hasDynamicOffset = true;
	bindingLayout.buffer.minBindingSize = 0;

	bindingLayout.sampler.nextInChain = nullptr;
	bindingLayout.sampler.type = WGPUSamplerBindingType_Undefined;

	bindingLayout.texture.nextInChain = nullptr;
	bindingLayout.texture.sampleType = WGPUTextureSampleType_Undefined;
	bindingLayout.texture.viewDimension = WGPUTextureViewDimension_Undefined;
	bindingLayout.texture.multisampled = false;

	bindingLayout.storageTexture.nextInChain = nullptr;
	bindingLayout.storageTexture.access = WGPUStorageTextureAccess_Undefined;
	bindingLayout.storageTexture.format = WGPUTextureFormat_Undefined;
	bindingLayout.storageTexture.viewDimension = WGPUTextureViewDimension_Undefined;
}

void Application::setDefault(WGPUStencilFaceState& stencilFaceState) const
{
	stencilFaceState.compare = WGPUCompareFunction_Always;
	stencilFaceState.failOp = WGPUStencilOperation_Keep;
	stencilFaceState.depthFailOp = WGPUStencilOperation_Keep;
	stencilFaceState.passOp = WGPUStencilOperation_Keep;
}

void Application::setDefault(WGPUDepthStencilState& depthStencilState) const
{
	depthStencilState.nextInChain = nullptr;
	depthStencilState.format = WGPUTextureFormat_Undefined;
	depthStencilState.depthWriteEnabled = false;
	depthStencilState.depthCompare = WGPUCompareFunction_Always;
	setDefault(depthStencilState.stencilFront);
	setDefault(depthStencilState.stencilBack);
	depthStencilState.stencilReadMask = 0xFFFFFFFF;
	depthStencilState.stencilWriteMask = 0xFFFFFFFF;
	depthStencilState.depthBias = 0;
	depthStencilState.depthBiasSlopeScale = 0;
	depthStencilState.depthBiasClamp = 0;
}

void Application::displayAdapterInfo(WGPUAdapter adapter)
{
	// Limits

	WGPUSupportedLimits adapterLimits = {};
	bool success = wgpuAdapterGetLimits(adapter, &adapterLimits) & WGPUStatus_Success;
	if (success) {
		SPDLOG_INFO("\nAdapter limits:\n - maxTextureDimension1D: {}\n - maxTextureDimension2D: {}\n - maxTextureDimension3D: {}\n - maxTextureArrayLayers: {}",\
					adapterLimits.limits.maxTextureDimension1D, adapterLimits.limits.maxTextureDimension2D, adapterLimits.limits.maxTextureDimension3D, adapterLimits.limits.maxTextureArrayLayers);
	}

	// Features

	std::vector<WGPUFeatureName> adapterFeat;
	size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
	adapterFeat.resize(featureCount);
	wgpuAdapterEnumerateFeatures(adapter, adapterFeat.data());

	std::stringstream ss;
	ss << "\nAdapter features:";
	std::cout << std::hex;
	for (auto feat : adapterFeat) {
		ss << "\n - 0x" << feat;
	}
	std::cout << std::dec;

	// Properties

	WGPUAdapterProperties adapterProp = {};
	adapterProp.nextInChain = nullptr;
	wgpuAdapterGetProperties(adapter, &adapterProp);

	ss << "\nAdapter properties:" <<
		"\n - vendorID: " << adapterProp.vendorID << "\n";
	if (adapterProp.vendorName) {
		ss << " - vendorName: " << adapterProp.vendorName << "\n";
	}
	if (adapterProp.architecture) {
		ss << " - architecture: " << adapterProp.architecture << "\n";
	}
	ss << " - deviceID: " << adapterProp.deviceID << "\n";
	if (adapterProp.name) {
		ss << " - name: " << adapterProp.name << "\n";
	}
	if (adapterProp.driverDescription) {
		ss << " - driverDescription: " << adapterProp.driverDescription << "\n";
	}

	std::cout << std::hex;
	ss << " - adapterType: 0x" << adapterProp.adapterType <<
		"\n - backendType: 0x" << adapterProp.backendType << "\n";
	std::cout << std::dec;

	SPDLOG_INFO("{}", ss.str());
}

void Application::inspectDevice(WGPUDevice device)
{
	std::vector<WGPUFeatureName> deviceFeat;
	size_t featureCount = wgpuDeviceEnumerateFeatures(device, nullptr);
	deviceFeat.resize(featureCount);
	wgpuDeviceEnumerateFeatures(device, deviceFeat.data());

	std::stringstream ss;
	ss << "\nDevice features:\n";
	std::cout << std::hex;
	for (auto feat : deviceFeat) {
		ss << " - 0x" << feat << "\n";
	}
	std::cout << std::dec;

	WGPUSupportedLimits deviceLimits = {};
	deviceLimits.nextInChain = nullptr;

	bool success = wgpuDeviceGetLimits(device, &deviceLimits) & WGPUStatus_Success;
	if (success) {
		ss << "Device limits: " <<
			"\n - maxTextureDimension1D: " << deviceLimits.limits.maxTextureDimension1D <<
			"\n - maxTextureDimension2D: " << deviceLimits.limits.maxTextureDimension2D <<
			"\n - maxTextureDimension3D: " << deviceLimits.limits.maxTextureDimension3D <<
			"\n - maxTextureArrayLayers: " << deviceLimits.limits.maxTextureArrayLayers << "\n";
	}

	// IMPORTANT FOR UNIFORMS IN SHADER!
	m_uniformStride = ceilToNextMultiple((uint32_t)sizeof(MyUniforms), (uint32_t)deviceLimits.limits.minUniformBufferOffsetAlignment);

	SPDLOG_INFO("{}", ss.str());
}

std::pair<WGPUSurfaceTexture, WGPUTextureView> Application::getNextSurfaceViewData()
{
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);
	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
		return {surfaceTexture, nullptr};
	}

	WGPUTextureViewDescriptor viewDesc = {};
	viewDesc.nextInChain = nullptr;
	viewDesc.label = "Surface texture view";
	viewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.baseMipLevel = 0;
	viewDesc.mipLevelCount = 1;
	viewDesc.baseArrayLayer = 0;
	viewDesc.arrayLayerCount = 1;
	viewDesc.aspect = WGPUTextureAspect_All;
	
	WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

	return {surfaceTexture, targetView};
}

bool Application::Initialize()
{
	// Instance creation

	// Dawn debugging/breakpoint stuff (get around Dawn's tick system)
	// https://dawn.googlesource.com/dawn/+/HEAD/docs/dawn/features/
	WGPUDawnTogglesDescriptor toggles;
	toggles.chain.next = nullptr;
	toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
	toggles.disabledToggleCount = 0;
	toggles.enabledToggleCount = 1;
	const char* toggleName = "enable_immediate_error_handling";
	toggles.enabledToggles = &toggleName;

	WGPUInstanceFeatures instanceFeat = {};
	instanceFeat.nextInChain = nullptr;
	instanceFeat.timedWaitAnyEnable = false;
	instanceFeat.timedWaitAnyMaxCount = 1;
	WGPUInstanceDescriptor instanceDesc = {};
	instanceDesc.nextInChain = &toggles.chain;
	instanceDesc.features = instanceFeat;
	m_instance = wgpuCreateInstance(&instanceDesc);
	if (!m_instance) {
		SPDLOG_ERROR("WebGPU instance was not created.");
		exit(1);
	}

	// Instance adapter

	SPDLOG_INFO("Requesting adapter...");
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	adapterOpts.compatibleSurface = nullptr;
	// adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
	adapterOpts.backendType = WGPUBackendType_Vulkan;
	m_adapter = requestAdapterSync(m_instance, &adapterOpts);
	SPDLOG_INFO("Created adapter.");
	displayAdapterInfo(m_adapter);

	// Device creation

	WGPURequiredLimits requiredLimits = getRequiredLimits(m_adapter);

	SPDLOG_INFO("Requesting device...");
	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The Default Queue";
	deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* device, WGPUDeviceLostReason reason, char const* message, void* pUserData)
	{
		Application& app = *reinterpret_cast<Application*>(pUserData);

		if (!app.IsRunning() && reason == WGPUDeviceLostReason_InstanceDropped) {
			SPDLOG_INFO("Device shutdown during the application termination function (expected).");
		} else {
			SPDLOG_ERROR("Device lost: reason: {}.", (int)reason);
			if (message) {
				SPDLOG_ERROR("Error message: {}", message);
			}
		}
	};
	deviceDesc.deviceLostCallbackInfo.userdata = this;
	m_device = requestDeviceSync(m_adapter, &deviceDesc);
	SPDLOG_INFO("Created device.");
	inspectDevice(m_device);

	// Device error callback

	auto onDeviceError = [](WGPUErrorType type, const char* message, void* /* pUserData */)
	{
		SPDLOG_ERROR("Uncaptured device error: type: {}", (int)type);
		// std::cout << "" << type;
		if (message) {
			// std::cout << " (" << message << ")";
			SPDLOG_ERROR("Error message: {}", message);
			exit(1);
		}
		// std::cout << "\n";
	};
	wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr);

	m_queue = wgpuDeviceGetQueue(m_device);

	// Init GLFW
	if (!glfwInit()) {
		SPDLOG_ERROR("Could not initialize GLFW");
		exit(1);
	}

	// Make sure we don't use a graphics API
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_glfwWindow = glfwCreateWindow(1920, 1080, "Graphics Midterm", nullptr, nullptr);
	if (!m_glfwWindow) {
		SPDLOG_ERROR("Failed to create a GLFW window.");
		exit(1);
	}

	// Surface configuration

	m_surface = glfwGetWGPUSurface(m_instance, m_glfwWindow);
	WGPUSurfaceConfiguration surfaceConfig = {};
	surfaceConfig.nextInChain = nullptr;
	surfaceConfig.device = m_device;
	
	WGPUSurfaceCapabilities surfaceCap = {};
	wgpuSurfaceGetCapabilities(m_surface, m_adapter, &surfaceCap);
	if (!surfaceCap.formats || surfaceCap.formatCount == 0) {
		SPDLOG_ERROR("There are no available formats.");
		exit(1);
	}

	m_surfaceFormat = surfaceCap.formats[0];
	SPDLOG_INFO("Using surface format: {:#x}", (uint32_t)m_surfaceFormat);

	surfaceConfig.format = m_surfaceFormat;
	surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
	surfaceConfig.width = 1920;
	surfaceConfig.height = 1080;
	surfaceConfig.presentMode = WGPUPresentMode_Fifo;

	wgpuSurfaceConfigure(m_surface, &surfaceConfig);

	// IMPORTANT: ORDER MATTERS HERE!!!!!
	initializeBuffers();
	initializePipeline();
	initializeBindGroups(); // Do this last

	if (!m_pipeline) {
		SPDLOG_ERROR("Failed to create a render pipeline!");
		exit(1);
	}

	return true;
}

void Application::initializeBindGroups()
{
	// 2. Create bind group entry (actual resource data)
	std::vector<WGPUBindGroupEntry> bindings(3);
	// Uniform buffer
	bindings[0].nextInChain = nullptr;
	bindings[0].binding = 0; // Index of binding
	bindings[0].buffer = m_uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(MyUniforms);
	// Texture
	bindings[1].nextInChain = nullptr;
	bindings[1].binding = 1;
	bindings[1].textureView = m_textureView;
	// Sampler
	bindings[2].nextInChain = nullptr;
	bindings[2].binding = 2;
	bindings[2].sampler = m_sampler;

	// 3. Create the actual bind group
	WGPUBindGroupDescriptor bindGroupDesc = {};
	bindGroupDesc.nextInChain = nullptr;
	bindGroupDesc.label = "My bind group descriptor";
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = static_cast<uint32_t>(bindings.size());
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindGroupDesc);
}

void Application::initializePipeline()
{
	// Create shader
	WGPUShaderModule shaderModule = ResourceManager::LoadShaderModule(RESOURCE_DIR "shader.wgsl", m_device);
	if (shaderModule == nullptr) {
		SPDLOG_ERROR("Failed to create shader module!");
		exit(1);
	}
	SPDLOG_INFO("Shader module created.");

	WGPURenderPipelineDescriptor pipelineDesc = {};
	pipelineDesc.nextInChain = nullptr;
	pipelineDesc.label = "Main pipeline";
	
	// Layout
	
	// 1. Create bind group layout entries
	std::vector<WGPUBindGroupLayoutEntry> bindingLayoutEntries(3);

	// For the uniform buffer
	WGPUBindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
	setDefault(bindingLayout);
	bindingLayout.binding = 0;
	bindingLayout.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms); // Need multiple of 16 for uniform buffer

	// For the texture
	WGPUBindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
	setDefault(textureBindingLayout);
	textureBindingLayout.binding = 1;
	textureBindingLayout.visibility = WGPUShaderStage_Fragment;
	textureBindingLayout.texture.sampleType = WGPUTextureSampleType_Float; // What variable type will be returned when sampling the texture
	textureBindingLayout.texture.viewDimension = WGPUTextureViewDimension_2D;

	// For the sampler
	WGPUBindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
	setDefault(samplerBindingLayout);
	samplerBindingLayout.binding = 2;
	samplerBindingLayout.visibility = WGPUShaderStage_Fragment;
	samplerBindingLayout.sampler.nextInChain = nullptr;
	samplerBindingLayout.sampler.type = WGPUSamplerBindingType_Filtering;

	// 2. Create bind group layout (blueprint)
	WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
	bindGroupLayoutDesc.nextInChain = nullptr;
	bindGroupLayoutDesc.label = "My main binding group layout";
	bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEntries.size()); // Just the u_time for now
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	m_bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bindGroupLayoutDesc);

	WGPUPipelineLayoutDescriptor layoutDesc = {};
	layoutDesc.nextInChain = nullptr;
	layoutDesc.label = "Main pipeline layout";
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = &m_bindGroupLayout;

	SPDLOG_INFO("Creating pipeline layout...");
	m_layout = wgpuDeviceCreatePipelineLayout(m_device, &layoutDesc);
	if (!m_layout) {
		SPDLOG_ERROR("Failed to create pipeline layout!");
		exit(1);
	}
	SPDLOG_INFO("Pipeline layout created");

	// 3. Insert into pipeline
	pipelineDesc.layout = m_layout;

	// Vertex

	std::vector<WGPUVertexAttribute> vertexAttribs(4);
	// Position attribute
	vertexAttribs[0].format = WGPUVertexFormat_Float32x3; // 3 32-bit floats for X, Y, (Z)
	vertexAttribs[0].offset = offsetof(VertexAttributes, position);
	vertexAttribs[0].shaderLocation = 0; // @location(0)
	// Normal attribute
	vertexAttribs[1].format = WGPUVertexFormat_Float32x3;
	vertexAttribs[1].offset = offsetof(VertexAttributes, normal);
	vertexAttribs[1].shaderLocation = 1; // @location(1)
	// Color attribute
	vertexAttribs[2].format = WGPUVertexFormat_Float32x3;
	vertexAttribs[2].offset = offsetof(VertexAttributes, color);
	vertexAttribs[2].shaderLocation = 2; // @location(2)
	// UV attribute
	vertexAttribs[3].format = WGPUVertexFormat_Float32x2;
	vertexAttribs[3].offset = offsetof(VertexAttributes, uv);
	vertexAttribs[3].shaderLocation = 3;

	WGPUVertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.arrayStride = sizeof(VertexAttributes); // 11 attributes: (X, Y, Z), (NX, NY, NZ), (R, G, B), and (U, V)
	vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();

	pipelineDesc.vertex.nextInChain = nullptr;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;
	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;

	// Primitive
	pipelineDesc.primitive.nextInChain = nullptr;
	pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList; // Each 3 points is a triangle (GL_TRIANGLE)
	pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined; // Sequential connections
	pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW; // The face oritentation is enumerated in CCW order
	pipelineDesc.primitive.cullMode = WGPUCullMode_None; // No optimizations yet

	// Depth stencil (ASSUMED DEFAULTS)

	// 1. Create stencil state
	WGPUDepthStencilState depthStencilState = {};
	setDefault(depthStencilState);
	WGPUTextureFormat depthTextureFormat = WGPUTextureFormat_Depth24Plus;
	depthStencilState.format = m_depthTextureFormat;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.depthCompare = WGPUCompareFunction_Less; // Blend if current depth value is less than the one stored in the Z-Buffer
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;
	
	// 2. Insert into pipeline
	pipelineDesc.depthStencil = &depthStencilState;

	// Multisampling

	pipelineDesc.multisample.nextInChain = nullptr;
	pipelineDesc.multisample.count = 1; // One subelement (No AA)
	pipelineDesc.multisample.mask = ~0u; // All bits on
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	// Fragment

	WGPUBlendState blendState = {};
	blendState.color.operation = WGPUBlendOperation_Add;
	blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blendState.alpha.operation = WGPUBlendOperation_Add;
	blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
	blendState.alpha.dstFactor = WGPUBlendFactor_One;

	WGPUColorTargetState colorTarget = {};
	colorTarget.nextInChain = nullptr;
	colorTarget.format = m_surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = WGPUColorWriteMask_All; // Only write to the same number of color channels

	WGPUFragmentState fragState = {};
	fragState.nextInChain = nullptr;
	fragState.module = shaderModule;
	fragState.entryPoint = "fs_main";
	fragState.constantCount = 0;
	fragState.constants = nullptr;
	fragState.targetCount = 1;
	fragState.targets = &colorTarget;

	pipelineDesc.fragment = &fragState;

	// Create the render pipeline
	SPDLOG_INFO("Creating render pipeline...\n  - Vertex entry point: {}\n  - Fragment entry point: {}\n  - Color target format: {:#x}.", \
				pipelineDesc.vertex.entryPoint, fragState.entryPoint, (int)colorTarget.format);

	// Create the pipeline
	m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);

	// Discard after binding to pipeline
	wgpuShaderModuleRelease(shaderModule);

	if (!m_pipeline) {
		SPDLOG_ERROR("wgpuDeviceCreateRenderPipeline returned nullptr!");
		exit(1);
	} else {
		SPDLOG_INFO("Render pipeline created.");
	}
}

void Application::initializeBuffers()
{
	std::vector<uint16_t> indexData;
	bool success = ResourceManager::LoadGeometryFromObj(RESOURCE_DIR "fourareen.obj", m_vertexData);
	if (!success) {
		SPDLOG_ERROR("Could not load geometry!");
		exit(1);
	}

	// Vertex buffer
	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.nextInChain = nullptr;
	bufferDesc.label = "My main vertex buffer";
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; // Can only copy TO and not FROM
	bufferDesc.size = m_vertexData.size() * sizeof(VertexAttributes); // MAKE SURE THAT WE DON'T REQUEST SIZE (buffer) > MAXBUFFERSIZE (device)
	bufferDesc.mappedAtCreation = false;
	m_vertexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0, m_vertexData.data(), bufferDesc.size);

	m_indexCount = static_cast<uint32_t>(m_vertexData.size());

	// Index buffer
	// bufferDesc.label = "My main index buffer";
	// bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
	// bufferDesc.size = indexData.size() * sizeof(uint16_t);
	// bufferDesc.size = (bufferDesc.size + 3) & ~3; // Round up to the next multiple of 4 (REQUIRED BY WEBGPU)
	// indexData.resize((indexData.size() + 1) & ~1); // Round up to the next multiple of 2
	// m_indexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	// wgpuQueueWriteBuffer(m_queue, m_indexBuffer, 0, indexData.data(), bufferDesc.size);

	// Uniform buffer
	bufferDesc.label = "My main uniform buffer";
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	bufferDesc.size = m_uniformStride + sizeof(MyUniforms); // IMPORTANT! MAKE SURE WE ARE USING MULTIPLES OF 16
	m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);

	calculateUniforms();

	//// Upload first value
	//m_uniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
	//m_uniforms.time = 1.0f;
	//wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, offsetof(), &m_uniforms, sizeof(MyUniforms));

	//// Second value
	//m_uniforms.color = {1.0f, 1.0f, 1.0f, 0.7f};
	//m_uniforms.time = -1.0f;
	//wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, m_uniformStride, &m_uniforms, sizeof(MyUniforms));

	// 0. Update class members
	m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;

	// 1. Create resources

	m_textureView = nullptr;
	m_texture = ResourceManager::LoadTexture(RESOURCE_DIR "fourareen2K_albedo.jpg", m_device, &m_textureView);
	if (!m_texture) {
		SPDLOG_ERROR("Could not load texture!");
		exit(1);
	}

	// Regular texture
	//WGPUTextureDescriptor textureDesc = {};
	//textureDesc.nextInChain = nullptr;
	//textureDesc.label = "My main texture";
	//textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	//textureDesc.dimension = WGPUTextureDimension_2D;
	//textureDesc.size = {256, 256, 1};
	//textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
	//textureDesc.mipLevelCount = 8; // 8 mip maps
	//textureDesc.sampleCount = 1;
	//textureDesc.viewFormatCount = 0;
	//textureDesc.viewFormats = nullptr;
	//m_texture = wgpuDeviceCreateTexture(m_device, &textureDesc);

	//WGPUTextureViewDescriptor textureViewDesc = {};
	//textureViewDesc.nextInChain = nullptr;
	//textureViewDesc.label = "My main texture view";
	//textureViewDesc.format = textureDesc.format;
	//textureViewDesc.dimension = WGPUTextureViewDimension_2D;
	//textureViewDesc.baseMipLevel = 0;
	//textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
	//textureViewDesc.baseArrayLayer = 0;
	//textureViewDesc.arrayLayerCount = 1;
	//textureViewDesc.aspect = WGPUTextureAspect_All;
	//m_textureView = wgpuTextureCreateView(m_texture, &textureViewDesc);

	//WGPUImageCopyTexture destination;
	//destination.texture = m_texture;
	//destination.mipLevel = 0;
	//destination.origin = {0, 0, 0}; // Offset for writeBuffer (what part of the image do we change)
	//destination.aspect = WGPUTextureAspect_All; // Only relevant to depth/stencil textures

	//WGPUTextureDataLayout source;
	//source.nextInChain = nullptr;
	//source.offset = 0;
	//
	//WGPUExtent3D mipLevelSize = textureDesc.size;
	//std::vector<uint8_t> previousLevelPixels;
	//for (uint32_t level = 0; level < textureDesc.mipLevelCount; ++level) {
	//	// The entire image for the current mip level
	//	std::vector<uint8_t> pixels(4 * mipLevelSize.width * mipLevelSize.height);
	//	for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
	//		for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
	//			uint8_t* p = &pixels[4 * (j * mipLevelSize.width + i)];
	//			if (level == 0) {
	//				// Our initial texture formula
	//				p[0] = (i / 16) % 2 == (j / 16) % 2 ? 255 : 0; // r
	//				p[1] = ((i - j) / 16) % 2 == 0 ? 255 : 0; // g
	//				p[2] = ((i + j) / 16) % 2 == 0 ? 255 : 0; // b
	//			} else {
	//				// Get the corresponding 4 pixels from the previous level
	//				uint8_t* p00 = &previousLevelPixels[4 * ((2 * j + 0) * (2 * mipLevelSize.width) + (2 * i + 0))];
	//				uint8_t* p01 = &previousLevelPixels[4 * ((2 * j + 0) * (2 * mipLevelSize.width) + (2 * i + 1))];
	//				uint8_t* p10 = &previousLevelPixels[4 * ((2 * j + 1) * (2 * mipLevelSize.width) + (2 * i + 0))];
	//				uint8_t* p11 = &previousLevelPixels[4 * ((2 * j + 1) * (2 * mipLevelSize.width) + (2 * i + 1))];
	//				// Average
	//				p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
	//				p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
	//				p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
	//			}
	//			p[3] = 255; // a
	//		}
	//	}

	//	destination.mipLevel = level; // Current

	//	source.bytesPerRow = 4 * mipLevelSize.width;
	//	source.rowsPerImage = mipLevelSize.height;

	//	wgpuQueueWriteTexture(m_queue, &destination, pixels.data(), pixels.size(), &source, &mipLevelSize);

	//	mipLevelSize.width /= 2;
	//	mipLevelSize.height /= 2;
	//	previousLevelPixels = std::move(pixels);
	//}

	// Depth texture
	WGPUTextureDescriptor depthTextureDesc = {};
	depthTextureDesc.nextInChain = nullptr;
	depthTextureDesc.label = "My main depth texture";
	depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
	depthTextureDesc.dimension = WGPUTextureDimension_2D;
	depthTextureDesc.size = {1920, 1080, 1};
	depthTextureDesc.format = m_depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = &m_depthTextureFormat;
	m_depthTexture = wgpuDeviceCreateTexture(m_device, &depthTextureDesc);

	WGPUTextureViewDescriptor depthTextureViewDesc = {};
	depthTextureViewDesc.nextInChain = nullptr;
	depthTextureViewDesc.label = "My main depth texture view";
	depthTextureViewDesc.format = m_depthTextureFormat;
	depthTextureViewDesc.dimension = WGPUTextureViewDimension_2D;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.aspect = WGPUTextureAspect_DepthOnly;
	m_depthTextureView = wgpuTextureCreateView(m_depthTexture, &depthTextureViewDesc);

	// 2. Create the sampler
	WGPUSamplerDescriptor samplerDesc = {};
	samplerDesc.nextInChain = nullptr;
	samplerDesc.label = "My main sampler";
	samplerDesc.addressModeU = WGPUAddressMode_Repeat;
	samplerDesc.addressModeV = WGPUAddressMode_Repeat;
	samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
	samplerDesc.magFilter = WGPUFilterMode_Linear;
	samplerDesc.minFilter = WGPUFilterMode_Linear;
	samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
	samplerDesc.lodMinClamp = 0.0f;
	samplerDesc.lodMaxClamp = 8.0f;
	samplerDesc.compare = WGPUCompareFunction_Undefined;
	samplerDesc.maxAnisotropy = 1;
	m_sampler = wgpuDeviceCreateSampler(m_device, &samplerDesc);
}

void Application::calculateUniforms()
{
	// Model transformation matrix

	float angle1 = (float)glfwGetTime();
	float angle2 = 3.0 * PI / 4.0;

	// glm::mat4x4 S = glm::scale(glm::mat4x4(1.0), glm::vec3(0.3f));
	// glm::mat4x4 T1 = glm::translate(glm::mat4x4(1.0), glm::vec3(0.5, 0.0, 0.0));
	// glm::mat4x4 R1 = glm::rotate(glm::mat4x4(1.0), angle1, glm::vec3(0.0, 0.0, 1.0));
	// m_uniforms.modelMatrix = R1 * T1 * S;
	m_uniforms.modelMatrix = glm::mat4x4(1.0);

	// glm::vec3 focalPoint = glm::vec3(0.0, 0.0, -2.0);
	// float focalLength = 2.0f;
	// glm::mat4x4 R2 = glm::rotate(glm::mat4x4(1.0), -angle2, glm::vec3(1.0, 0.0, 0.0));
	// glm::mat4x4 T2 = glm::translate(glm::mat4x4(1.0), -focalPoint);
	// m_uniforms.viewMatrix = T2 * R2;
	m_uniforms.viewMatrix = glm::lookAt(glm::vec3(-2.0f, -3.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1));

	// float near = 0.001f;
	// float far = 100.0f;
	// float ratio = 1920.0f / 1080.0f;
	// float fov = 2 * glm::atan(1 / focalLength);
	// m_uniforms.projectionMatrix = glm::perspective(fov, ratio, near, far);
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, 1920.0f / 1080.0f, 0.01f, 100.0f);

	// Other uniforms
	m_uniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
	m_uniforms.time = 1.0f;

	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, projectionMatrix), &m_uniforms.projectionMatrix, sizeof(MyUniforms::projectionMatrix));
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, viewMatrix), &m_uniforms.viewMatrix, sizeof(MyUniforms::viewMatrix));
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, modelMatrix), &m_uniforms.modelMatrix, sizeof(MyUniforms::modelMatrix));
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, color), &m_uniforms.color, sizeof(MyUniforms::color));
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));
}

void Application::MainLoop()
{
	glfwPollEvents();
	wgpuInstanceProcessEvents(m_instance);
	wgpuDeviceTick(m_device);

	// 0. Update buffers (only upload time to MyUniforms, which is the first 4 bytes)
	// TODO: Optimize
	// float t = static_cast<float>(glfwGetTime());
	// glm::mat4x4 S = glm::scale(glm::mat4x4(1.0), glm::vec3(0.3f));
	// glm::mat4x4 T1 = glm::translate(glm::mat4x4(1.0), glm::vec3(0.5, 0.0, 0.0));
	// glm::mat4x4 R1 = glm::rotate(glm::mat4x4(1.0), t, glm::vec3(0.0, 0.0, 1.0));
	// m_uniforms.modelMatrix = R1 * T1 * S;
	// m_uniforms.time = t;
	// wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, modelMatrix), &m_uniforms.modelMatrix, sizeof(MyUniforms::modelMatrix));
	// wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 1 * m_uniformStride + offsetof(MyUniforms, time), &t, sizeof(float));
	

	// float viewZ = glm::mix(0.0f, 0.25f, cos(2 * PI * glfwGetTime() / 4) * 0.5 + 0.5);
	// m_uniforms.viewMatrix = glm::lookAt(glm::vec3(-0.5f, -1.5f, viewZ + 0.25f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	// wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, offsetof(MyUniforms, viewMatrix), &m_uniforms.viewMatrix, sizeof(MyUniforms::viewMatrix));

	// 1. Get textures from our surface
	auto [surfaceTexture, targetView] = getNextSurfaceViewData();
	if (!targetView) return;

	// 2. Establish render pass & attachments
	WGPURenderPassColorAttachment colorAtt = {};
	colorAtt.nextInChain = nullptr;
	colorAtt.view = targetView;
	colorAtt.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
	colorAtt.resolveTarget = nullptr;
	colorAtt.loadOp = WGPULoadOp_Clear;
	colorAtt.storeOp = WGPUStoreOp_Store;
	colorAtt.clearValue = WGPUColor{0.5, 0.5, 0.5, 1.0};

	WGPURenderPassDepthStencilAttachment depthStencilAtt = {};
	depthStencilAtt.view = m_depthTextureView;
	depthStencilAtt.depthLoadOp = WGPULoadOp_Clear;
	depthStencilAtt.depthStoreOp = WGPUStoreOp_Store;
	depthStencilAtt.depthClearValue = 1.0f; // The back plane of the Z-Buffer
	depthStencilAtt.depthReadOnly = false;
	// Not used at the moment (IMPORTANT!!!!!!!!!!!!!!!!!! DAWN NEEDS TO HAVE THE STENCIL UNDEFINED IF WE DON'T USE IT)
	depthStencilAtt.stencilLoadOp = WGPULoadOp_Undefined;
	depthStencilAtt.stencilStoreOp = WGPUStoreOp_Undefined;
	depthStencilAtt.stencilClearValue = 0;
	depthStencilAtt.stencilReadOnly = true;

	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;
	renderPassDesc.label = "Main render pass";
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAtt;
	renderPassDesc.depthStencilAttachment = &depthStencilAtt;
	renderPassDesc.occlusionQuerySet = nullptr;
	renderPassDesc.timestampWrites = nullptr;
	
	// 3. Command encoder & render pass
	WGPUCommandEncoderDescriptor cmdEncoderDesc = {};
	cmdEncoderDesc.nextInChain = nullptr;
	cmdEncoderDesc.label = "Main command encoder";
	WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(m_device, &cmdEncoderDesc);
	WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);

	// Issue draw calls starting here
	wgpuRenderPassEncoderSetPipeline(renderPassEncoder, m_pipeline);

	// Set vertex buffer while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, m_vertexBuffer, 0, m_vertexData.size() * sizeof(VertexAttributes));
	// wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, m_indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_indexBuffer));

	// Set binding group here!
	uint32_t dynamicOffset = 0 * m_uniformStride;
	wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, m_bindGroup, 1, &dynamicOffset);
	// wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, m_indexCount, 1, 0, 0, 0);
	wgpuRenderPassEncoderDraw(renderPassEncoder, m_indexCount, 1, 0, 0);
	
	// Leave out for now
	// dynamicOffset = 1 * m_uniformStride;
	// wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, m_bindGroup, 1, &dynamicOffset);
	// wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, m_indexCount, 1, 0, 0, 0);

	wgpuRenderPassEncoderEnd(renderPassEncoder);
	wgpuRenderPassEncoderRelease(renderPassEncoder);

	WGPUCommandBufferDescriptor cmdBuffDesc = {};
	cmdBuffDesc.nextInChain = nullptr;
	cmdBuffDesc.label = "Main command buffer";
	WGPUCommandBuffer cmdBuff = wgpuCommandEncoderFinish(cmdEncoder, &cmdBuffDesc);
	wgpuCommandEncoderRelease(cmdEncoder);

	// 4. Submit commands
	m_gpuIdle = false;

	auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* pUserData1, void* /* pUserData2 */)
	{
		// std::cout << "Queued work finished with status: " << status << "\n";
		bool& gpuIdle = *reinterpret_cast<bool*>(pUserData1);
		gpuIdle = true;
	};

	WGPUQueueWorkDoneCallbackInfo2 queueCBInfo = {};
	queueCBInfo.nextInChain = nullptr;
	queueCBInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	queueCBInfo.callback = onQueueWorkDone;
	queueCBInfo.userdata1 = (void*)&m_gpuIdle;
	queueCBInfo.userdata2 = nullptr;

	wgpuQueueOnSubmittedWorkDone2(m_queue, queueCBInfo);
	wgpuQueueSubmit(m_queue, 1, &cmdBuff);
	wgpuCommandBufferRelease(cmdBuff);

	// 6. Present rendered surface
	wgpuSurfacePresent(m_surface);

	// 7. Release surface after render
	wgpuTextureRelease(surfaceTexture.texture);
	wgpuTextureViewRelease(targetView);
}

void Application::Terminate()
{
	while (!m_gpuIdle) {
		wgpuInstanceProcessEvents(m_instance); // Process events for callbacks
		wgpuDeviceTick(m_device); // Tick the device to process internal work

		WGPUFutureWaitInfo waitInfo = {};
		waitInfo.future = {};
		waitInfo.completed = false;

		WGPUWaitStatus waitStatus = wgpuInstanceWaitAny(m_instance, UINT64_MAX, &waitInfo, 1);
		if (waitStatus == WGPUWaitStatus_TimedOut) {
			SPDLOG_WARN("GPU work timeout during termination.");
			break;
		}
	}

	SPDLOG_INFO("GPU work completed, cleaning up resources...");

	wgpuRenderPipelineRelease(m_pipeline);

	wgpuPipelineLayoutRelease(m_layout);
	wgpuBindGroupRelease(m_bindGroup);
	wgpuBindGroupLayoutRelease(m_bindGroupLayout);

	wgpuBufferRelease(m_uniformBuffer);
	// wgpuBufferRelease(m_indexBuffer);
	wgpuBufferRelease(m_vertexBuffer);

	// Check if we can release stuff here?
	wgpuTextureDestroy(m_texture);
	wgpuTextureRelease(m_texture);
	wgpuTextureViewRelease(m_depthTextureView);
	wgpuTextureDestroy(m_depthTexture);
	wgpuTextureRelease(m_depthTexture);

	wgpuSurfaceUnconfigure(m_surface);
	wgpuSurfaceRelease(m_surface);
	wgpuQueueRelease(m_queue);
	wgpuDeviceDestroy(m_device);
	wgpuDeviceRelease(m_device);
	wgpuAdapterRelease(m_adapter);
	wgpuInstanceRelease(m_instance);

	glfwDestroyWindow(m_glfwWindow);
	glfwTerminate();

	SPDLOG_INFO("Application terminated successfully.");
}

bool Application::IsRunning()
{
	return !glfwWindowShouldClose(m_glfwWindow);
}

int main()
{
	Application app;

	if (!app.Initialize()) {
		return 1;
	}
	while (app.IsRunning()) {
		app.MainLoop(); 
	}
	app.Terminate();
	
	return 0;
}