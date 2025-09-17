#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>
#include <array>
#include <thread>

#include <glfw3webgpu.h>
#include "Main.hpp"

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
            std::cerr << "Could not get WebGPU adapter: " << message << "\n";
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
            std::cerr << "Could not get the WebGPU device: " << message << "\n";
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

void DisplayAdapterInfo(WGPUAdapter adapter)
{
    // Limits

    WGPUSupportedLimits adapterLimits = {};
    bool success = wgpuAdapterGetLimits(adapter, &adapterLimits) & WGPUStatus_Success;
    if (success) {
        std::cout << "\nAdapter limits:" <<
            "\n - maxTextureDimension1D: " << adapterLimits.limits.maxTextureDimension1D <<
            "\n - maxTextureDimension2D: " << adapterLimits.limits.maxTextureDimension2D <<
            "\n - maxTextureDimension3D: " << adapterLimits.limits.maxTextureDimension3D <<
            "\n - maxTextureArrayLayers: " << adapterLimits.limits.maxTextureArrayLayers << "\n";
    }

    // Features

    std::vector<WGPUFeatureName> adapterFeat;

    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    adapterFeat.resize(featureCount);
    wgpuAdapterEnumerateFeatures(adapter, adapterFeat.data());

    std::cout << "\nAdapter features:\n" << std::hex;
    for (auto feat : adapterFeat) {
        std::cout << " - 0x" << feat << "\n";
    }
    std::cout << std::dec;

    // Properties

    WGPUAdapterProperties adapterProp = {};
    adapterProp.nextInChain = nullptr;
    wgpuAdapterGetProperties(adapter, &adapterProp);

    std::cout << "\nAdapter properties:" <<
        "\n - vendorID: " << adapterProp.vendorID << "\n";
    if (adapterProp.vendorName) {
        std::cout << " - vendorName: " << adapterProp.vendorName << "\n";
    }
    if (adapterProp.architecture) {
        std::cout << " - architecture: " << adapterProp.architecture << "\n";
    }
    std::cout << " - deviceID: " << adapterProp.deviceID << "\n";
    if (adapterProp.name) {
        std::cout << " - name: " << adapterProp.name << "\n";
    }
    if (adapterProp.driverDescription) {
        std::cout << " - driverDescription: " << adapterProp.driverDescription << "\n";
    }

    std::cout << std::hex;
    std::cout << " - adapterType: 0x" << adapterProp.adapterType <<
        "\n - backendType: 0x" << adapterProp.backendType << "\n";
    std::cout << std::dec;
}

void InspectDevice(WGPUDevice device)
{
    std::vector<WGPUFeatureName> deviceFeat;
    size_t featureCount = wgpuDeviceEnumerateFeatures(device, nullptr);
    deviceFeat.resize(featureCount);
    wgpuDeviceEnumerateFeatures(device, deviceFeat.data());

    std::cout << "\nDevice features:\n" << std::hex;
    for (auto feat : deviceFeat) {
        std::cout << " - 0x" << feat << "\n";
    }
    std::cout << std::dec;

    WGPUSupportedLimits deviceLimits = {};
    deviceLimits.nextInChain = nullptr;

    bool success = wgpuDeviceGetLimits(device, &deviceLimits) & WGPUStatus_Success;
    if (success) {
        std::cout << "Device limits: " <<
            "\n - maxTextureDimension1D: " << deviceLimits.limits.maxTextureDimension1D <<
            "\n - maxTextureDimension2D: " << deviceLimits.limits.maxTextureDimension2D <<
            "\n - maxTextureDimension3D: " << deviceLimits.limits.maxTextureDimension3D <<
            "\n - maxTextureArrayLayers: " << deviceLimits.limits.maxTextureArrayLayers << "\n";
    }
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
        std::cerr << "Error: WebGPU instance was not created.\n";
        return false;
    }

    // Instance adapter

    std::cout << "Requesting adapter...\n";
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = nullptr;
    adapterOpts.backendType = WGPUBackendType_Vulkan;
    m_adapter = requestAdapterSync(m_instance, &adapterOpts);
    std::cout << "Got adapter: " << m_adapter << "\n";
    DisplayAdapterInfo(m_adapter);

    // Device creation

    std::cout << "\nRequesting device...\n";
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The Default Queue";
    deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* device, WGPUDeviceLostReason reason, char const* message, void* pUserData)
    {
        Application& app = *reinterpret_cast<Application*>(pUserData);

        if (!app.IsRunning() && reason == WGPUDeviceLostReason_InstanceDropped) {
            std::cout << "Device shutdown during the application termination function (expected)\n";
        } else {
            std::cout << "Device " << device << " lost: reason: " << reason;
            if (message) {
                std::cout << " (" << message << ")";
            }
            std::cout << "\n";
        }
    };
    deviceDesc.deviceLostCallbackInfo.userdata = this;
    m_device = requestDeviceSync(m_adapter, &deviceDesc);
    std::cout << "Got device: " << m_device << "\n";
    InspectDevice(m_device);

    // Device error callback

    auto onDeviceError = [](WGPUErrorType type, const char* message, void* /* pUserData */)
    {
        std::cout << "Uncaptured device error: type: " << type;
        if (message) {
            std::cout << " (" << message << ")";
        }
        std::cout << "\n";
    };
    wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr);

    m_queue = wgpuDeviceGetQueue(m_device);

    // Init GLFW
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!\n";
        return false;
    }

    // Make sure we don't use a graphics API
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_glfwWindow = glfwCreateWindow(1920, 1080, "Graphics Midterm", nullptr, nullptr);
    if (!m_glfwWindow) {
        std::cerr << "Failed to create a GLFW window!\n";
        return false;
    }

    // Surface configuration

    m_surface = glfwGetWGPUSurface(m_instance, m_glfwWindow);
    WGPUSurfaceConfiguration surfaceConfig = {};
    surfaceConfig.nextInChain = nullptr;
    surfaceConfig.device = m_device;
    
    WGPUSurfaceCapabilities surfaceCap = {};
    wgpuSurfaceGetCapabilities(m_surface, m_adapter, &surfaceCap);
    if (!surfaceCap.formats) {
        std::cerr << "There are no available formats\n";
        return false;
    }

    surfaceConfig.format = surfaceCap.formats[0];
    surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
    surfaceConfig.width = 1920;
    surfaceConfig.height = 1080;
    surfaceConfig.presentMode = WGPUPresentMode_Fifo;

    wgpuSurfaceConfigure(m_surface, &surfaceConfig);
    
    return true;
}

void Application::MainLoop()
{
    glfwPollEvents();
    wgpuInstanceProcessEvents(m_instance);
    wgpuDeviceTick(m_device);

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
    colorAtt.clearValue = WGPUColor{0.9, 0.1, 0.2, 1.0};

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

    // Issue draw calls here before releasing the render pass encoders
    
    wgpuRenderPassEncoderEnd(renderPassEncoder);
    wgpuRenderPassEncoderRelease(renderPassEncoder);

    WGPUCommandBufferDescriptor cmdBuffDesc = {};
    cmdBuffDesc.nextInChain = nullptr;
    cmdBuffDesc.label = "Main command buffer";
    WGPUCommandBuffer cmdBuff = wgpuCommandEncoderFinish(cmdEncoder, &cmdBuffDesc);
    wgpuRenderPassEncoderRelease(renderPassEncoder);

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
            std::cout << "Warning: GPU work timeout during termination\n";
            break;
        }
    }

    std::cout << "GPU work completed, cleaning up resources...\n";

    wgpuSurfaceUnconfigure(m_surface);
    wgpuSurfaceRelease(m_surface);
    wgpuQueueRelease(m_queue);
    wgpuDeviceDestroy(m_device);
    wgpuDeviceRelease(m_device);
    wgpuAdapterRelease(m_adapter);
    wgpuInstanceRelease(m_instance);

    glfwDestroyWindow(m_glfwWindow);
    glfwTerminate();

    std::cout << "Application terminated successfully.\n";
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