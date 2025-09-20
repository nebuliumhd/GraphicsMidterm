#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>
#include <fstream>
#include <array>
#include <thread>

#include <spdlog/spdlog.h>
#include <glfw3webgpu.h>
#include "Main.hpp"
#include "ResourceManager.hpp"

#ifndef RESOURCE_DIR
#error "A RESOURCE_DIR must be defined to compile the project!"
#endif

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

	// Two vertex attributes (position and color)
	requiredLimits.limits.maxVertexAttributes = 2;
	// One vertex buffer
	requiredLimits.limits.maxVertexBuffers = 1;
	// A max of three floats forwarded from the vertex to the fragment shader
	requiredLimits.limits.maxInterStageShaderComponents = 3;
	// Minimum required buffer size needed (5 individual floats for each vertex of a triangle for two triangles)
	requiredLimits.limits.maxBufferSize = 2 * 3 * 5 * sizeof(float);
	// Maximum stride between consecutive vertices in a vertex buffer
	requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);

	// Uniforms

	// For texture uniforms
	requiredLimits.limits.maxBindGroups = 1;
	// Use at most 1 uniform buffer per stage
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	// Uniform structs have a size of a maximum 16 floats (way more than needed)
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * sizeof(float);
	// Extra limit requirement
	requiredLimits.limits.maxDynamicUniformBuffersPerPipelineLayout = 1;

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

	// Render pipeline
	// DO THESE BEFORE CREATING THE PIPELINE
	intializeBuffers();
	initializeBindGroups(); // The bind groups DEPEND on the buffers

	SPDLOG_INFO("Creating render pipeline...");
	initializePipeline();

	if (!m_pipeline) {
		SPDLOG_ERROR("Failed to create a render pipeline!");
		exit(1);
	}

	return true;
}

void Application::initializeBindGroups()
{
	// 1. Create bind group layout entry
	WGPUBindGroupLayoutEntry bindingLayout = {};
	setDefault(bindingLayout);
	bindingLayout.binding = 0;
	bindingLayout.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms); // Need multiple of 16 for uniform buffer

	// 2. Create bind group layout (blueprint)
	WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
	bindGroupLayoutDesc.nextInChain = nullptr;
	bindGroupLayoutDesc.label = "My main binding group layout";
	bindGroupLayoutDesc.entryCount = 1; // Just the u_time for now
	bindGroupLayoutDesc.entries = &bindingLayout;
	m_bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bindGroupLayoutDesc);

	// 3. Create bind group entry (actual resource data)
	WGPUBindGroupEntry binding = {};
	binding.nextInChain = nullptr;
	binding.binding = 0; // Index of binding
	binding.buffer = m_uniformBuffer;
	binding.offset = 0;
	binding.size = sizeof(MyUniforms);
	// Don't use other two for now

	// 4. Create the actual bind group
	WGPUBindGroupDescriptor bindGroupDesc = {};
	bindGroupDesc.nextInChain = nullptr;
	bindGroupDesc.label = "My bind group descriptor";
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = 1;
	bindGroupDesc.entries = &binding;
	m_bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindGroupDesc);

	// 5. Pipeline layout
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
}

void Application::initializePipeline()
{
	// Create shader
	WGPUShaderModule shaderModule = ResourceManager::LoadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
	if (shaderModule == nullptr) {
		SPDLOG_ERROR("Failed to create shader module!");
		exit(1);
	}
	SPDLOG_INFO("Shader module created.");

	WGPURenderPipelineDescriptor pipelineDesc = {};
	pipelineDesc.nextInChain = nullptr;
	pipelineDesc.label = "Main pipeline";
	
	// Layout (NEEDS TO HAVE BINDGROUPS INITIALIZED FIRST!!!)
	pipelineDesc.layout = m_layout;

	// Vertex buffer
	std::vector<WGPUVertexAttribute> vertexAttrib(2);
	vertexAttrib[0].format = WGPUVertexFormat_Float32x2; // 2 32-bit floats for X, Y
	vertexAttrib[0].offset = 0;
	vertexAttrib[0].shaderLocation = 0; // @location(0)
	vertexAttrib[1].format = WGPUVertexFormat_Float32x3;
	vertexAttrib[1].offset = 2 * sizeof(float); // Starts 2 
	vertexAttrib[1].shaderLocation = 1; // @location(1)

	WGPUVertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.arrayStride = 5 * sizeof(float); // Five attributes: (X, Y) and (R, G, B)
	vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttrib.size());
	vertexBufferLayout.attributes = vertexAttrib.data();

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
	pipelineDesc.depthStencil = nullptr;

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

void Application::intializeBuffers()
{
	std::vector<float> pointData;
	std::vector<uint16_t> indexData;
	bool success = ResourceManager::LoadGeometry(RESOURCE_DIR "webgpu.txt", pointData, indexData);
	if (!success) {
		SPDLOG_ERROR("Could not load geometry!");
		exit(1);
	}

	// Not needed?
	// m_vertexCount = static_cast<uint32_t>(pointData.size() / 5); // 5 is the number of COLLECTIVE vertex attributes
	m_indexCount = static_cast<uint32_t>(indexData.size());

	// Vertex buffer
	WGPUBufferDescriptor bufferDesc = {};
	bufferDesc.nextInChain = nullptr;
	bufferDesc.label = "My main vertex buffer";
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; // Can only copy TO and not FROM
	bufferDesc.size = pointData.size() * sizeof(float); // MAKE SURE THAT WE DON'T REQUEST SIZE (buffer) > MAXBUFFERSIZE (device)
	bufferDesc.mappedAtCreation = false;
	m_pointBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_pointBuffer, 0, pointData.data(), bufferDesc.size);

	// Index buffer
	bufferDesc.label = "My main index buffer";
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
	bufferDesc.size = indexData.size() * sizeof(uint16_t);
	bufferDesc.size = (bufferDesc.size + 3) & ~3; // Round up to the next multiple of 4 (REQUIRED BY WEBGPU)
	indexData.resize((indexData.size() + 1) & ~1); // Round up to the next multiple of 2
	m_indexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	wgpuQueueWriteBuffer(m_queue, m_indexBuffer, 0, indexData.data(), bufferDesc.size);

	// Uniform buffer
	bufferDesc.label = "My main uniform buffer";
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	bufferDesc.size = m_uniformStride + sizeof(MyUniforms); // IMPORTANT! MAKE SURE WE ARE USING MULTIPLES OF 16
	m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);

	MyUniforms uniforms;

	// Upload first value
	uniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
	uniforms.time = 1.0f;
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &uniforms, sizeof(MyUniforms));

	// Second value
	uniforms.color = {1.0f, 1.0f, 1.0f, 0.7f};
	uniforms.time = -1.0f;
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, m_uniformStride, &uniforms, sizeof(MyUniforms));
}

void Application::MainLoop()
{
	glfwPollEvents();
	wgpuInstanceProcessEvents(m_instance);
	wgpuDeviceTick(m_device);

	// 0. Update buffers (only upload time to MyUniforms, which is the first 4 bytes)
	float t = static_cast<float>(glfwGetTime());
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0 * m_uniformStride + offsetof(MyUniforms, time), &t, sizeof(float));
	// wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 1 * m_uniformStride + offsetof(MyUniforms, time), &t, sizeof(float));

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
	colorAtt.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};

	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;
	renderPassDesc.label = "Main render pass";
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAtt;
	renderPassDesc.depthStencilAttachment = nullptr;
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
	wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, m_pointBuffer, 0, wgpuBufferGetSize(m_pointBuffer));
	wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, m_indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_indexBuffer));

	// Set binding group here!
	uint32_t dynamicOffset = 0 * m_uniformStride;
	wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, m_bindGroup, 1, &dynamicOffset);
	wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, m_indexCount, 1, 0, 0, 0);
	
	dynamicOffset = 1 * m_uniformStride;
	wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, m_bindGroup, 1, &dynamicOffset);
	wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, m_indexCount, 1, 0, 0, 0);

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
	wgpuBufferRelease(m_pointBuffer);
	wgpuBufferRelease(m_indexBuffer);

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