#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>
#include <array>

#include <webgpu/webgpu.h>

WGPUInstance g_instance;
WGPUAdapter g_adapter;
WGPUDevice g_device;

WGPUQueue g_queue;

WGPUAdapter requestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options)
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

    // TODO: Add busy-waiting in case we have it become async

    assert(userData.requestEnded);

    return userData.adapter;
}

WGPUDevice requestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor)
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

    // TODO: Add busy-waiting in case we have it become async

    assert(userData.requestEnded);

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

void Init()
{
    // Instance creation

    WGPUInstanceFeatures instanceFeat = {};
    instanceFeat.nextInChain = nullptr;
    instanceFeat.timedWaitAnyEnable = false;
    instanceFeat.timedWaitAnyMaxCount = 1;

    WGPUInstanceDescriptor instanceDesc = {};

    // Dawn debugging/breakpoint stuff (get around Dawn's tick system)
    // https://dawn.googlesource.com/dawn/+/HEAD/docs/dawn/features/

    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.next = nullptr;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount = 1;
    const char* toggleName = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggleName;

    instanceDesc.nextInChain = &toggles.chain;
    instanceDesc.features = instanceFeat;
    g_instance = wgpuCreateInstance(&instanceDesc);
    if (!g_instance) {
        std::cerr << "Error: WebGPU instance was not created.\n";
    }

    // Instance adapter

    std::cout << "Requesting adapter...\n";

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.backendType = WGPUBackendType_Vulkan;
    g_adapter = requestAdapterSync(g_instance, &adapterOpts);
    
    std::cout << "Got adapter: " << g_adapter << "\n";

    DisplayAdapterInfo(g_adapter);

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
    deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* device, WGPUDeviceLostReason reason, char const* message, void* /* pUserData */)
    {
        std::cout << "Device " << device << " lost: reason: " << reason;
        if (message) {
            std::cout << " (" << message << ")";
        }
        std::cout << "\n";
    };

    g_device = requestDeviceSync(g_adapter, &deviceDesc);

    std::cout << "Got device: " << g_device << "\n";

    InspectDevice(g_device);

    // Device error callback

    auto onDeviceError = [](WGPUErrorType type, const char* message, void* /* pUserData */)
    {
        std::cout << "Uncaptured device error: type: " << type;
        if (message) {
            std::cout << " (" << message << ")";
        }
        std::cout << "\n";
    };
    wgpuDeviceSetUncapturedErrorCallback(g_device, onDeviceError, nullptr);

    // Command queue

    g_queue = wgpuDeviceGetQueue(g_device);
    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* pUserData1 */, void* /* pUserData2 */)
    {
        std::cout << "Queued work finished with status: " << status << "\n";
    };

    WGPUQueueWorkDoneCallbackInfo2 queueCBInfo = {};
    queueCBInfo.nextInChain = nullptr;
    queueCBInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    queueCBInfo.callback = onQueueWorkDone;
    queueCBInfo.userdata1 = nullptr;
    queueCBInfo.userdata2 = nullptr;

    wgpuQueueOnSubmittedWorkDone2(g_queue, queueCBInfo);
}

void DispatchCommands()
{
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "My command coder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &encoderDesc);

    wgpuCommandEncoderInsertDebugMarker(encoder, "Do one thing.");
    wgpuCommandEncoderInsertDebugMarker(encoder, "Do another thing.");

    WGPUCommandBufferDescriptor cmdDesc = {};
    cmdDesc.nextInChain = nullptr;
    cmdDesc.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdDesc);
    
    std::cout << "\nSubmitting command...\n";
    wgpuQueueSubmit(g_queue, 1, &command);
    wgpuCommandBufferRelease(command);
    std::cout << "Command submitted.\n";
}

void Shutdown()
{
    wgpuQueueRelease(g_queue);
    wgpuDeviceRelease(g_device);
    wgpuAdapterRelease(g_adapter);
    wgpuInstanceRelease(g_instance);
}

int main()
{
    Init();
    DispatchCommands();
    Shutdown();    
    return 0;
}