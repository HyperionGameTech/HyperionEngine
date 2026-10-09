/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPUSampler.hpp>
#include <Rendering/WebGPU/WebGPUFrame.hpp>
#include <Rendering/WebGPU/WebGPUFramebuffer.hpp>
#include <Rendering/WebGPU/WebGPUSwapchain.hpp>
#include <Rendering/WebGPU/WebGPUAsyncCompute.hpp>
#include <Rendering/WebGPU/WebGPUGpuTimerBackend.hpp>
#include <Rendering/WebGPU/WebGPUAccelerationStructure.hpp>
#include <Rendering/WebGPU/WebGPUDescriptorSet.hpp>
#include <Rendering/WebGPU/WebGPUGraphicsPipeline.hpp>
#include <Rendering/WebGPU/WebGPUComputePipeline.hpp>
#include <Rendering/WebGPU/WebGPURayTracingPipeline.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <Rendering/RenderHelpers.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/BLASCache.hpp>

#include <Core/Threading/AtomicFlag.hpp>
#include <Core/Threading/Threads.hpp>

#include <Framework/DeviceDetails.hpp>

#ifdef HYP_WEB
#include <emscripten.h>
#include <emscripten/threading.h>
#endif

#include <System/AppContext.hpp>

#include <Core/Logging/Logger.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

CVar<CVarString> g_cvWebGPUBackend("Rendering.WebGPU.Backend", "vulkan");

extern CVar<bool> g_cvIndirectRendering;

#pragma region WebGPURenderConfig

class WebGPURenderConfig final : public IRenderConfig
{
public:
    WebGPURenderConfig()
    {
        bindlessTextures = false;
        rayTracing = false;
        indirectRendering = g_cvIndirectRendering.Get();
        parallelRendering = false;
        dynamicDescriptorIndexing = false;
    }
};

#pragma endregion WebGPURenderConfig

#pragma region WebGPUSingleTimeCommands

class WebGPUSingleTimeCommands final : public SingleTimeCommands
{
public:
    WebGPUSingleTimeCommands() = default;
    virtual ~WebGPUSingleTimeCommands() override = default;

    virtual RendererResult Execute() override
    {
        AssertOnThread(g_renderThread);

        CommandRecorder cr;

        for (auto& fn : m_functions)
        {
            fn(cr);
        }

        m_functions.Clear();

        WebGPUCommandBuffer commandBuffer;
        CheckResultOrReturn(commandBuffer.Create());

        commandBuffer.Begin();
        cr.Execute(&commandBuffer);
        commandBuffer.End();

        RI.Submit(commandBuffer);
        RI.WaitForSubmittedWork();

        RI.stagingBufferPool->ReleaseForCommandBuffer(&commandBuffer);

        return {};
    }
};

#pragma endregion WebGPUSingleTimeCommands

#pragma region WebGPUTransientCommandBuffer

struct WebGPUTransientCommandBuffer
{
    WebGPUCommandBuffer commandBuffer;
};

#pragma endregion WebGPUTransientCommandBuffer

#pragma region Device callbacks

static void OnAdapterRequestEndedThunk(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status != WGPURequestAdapterStatus_Success)
    {
        HYP_LOG(RenderingBackend, Error, "WebGPU adapter request failed: {}", ToStringView(message));
    }

    static_cast<WebGPURenderInterface*>(userdata1)->OnAdapterRequestEnded(status == WGPURequestAdapterStatus_Success ? adapter : nullptr);
}

static void OnDeviceRequestEndedThunk(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status != WGPURequestDeviceStatus_Success)
    {
        HYP_LOG(RenderingBackend, Error, "WebGPU device request failed: {}", ToStringView(message));
    }

    static_cast<WebGPURenderInterface*>(userdata1)->OnDeviceRequestEnded(status == WGPURequestDeviceStatus_Success ? device : nullptr);
}

static void OnDeviceLost(WGPUDevice const* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (reason == WGPUDeviceLostReason_Destroyed)
    {
        return;
    }

    HYP_LOG(RenderingBackend, Error, "WebGPU device lost: {}", ToStringView(message));

    *static_cast<volatile bool*>(userdata1) = true;
}

static void OnUncapturedError(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
{
    HYP_LOG(RenderingBackend, Error, "WebGPU error ({}): {}", uint32(type), ToStringView(message));
}

static void OnSubmittedWorkDone(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void* userdata2)
{
    *static_cast<bool*>(userdata1) = true;
}

static void OnFrameWorkDone(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void* userdata2)
{
    static_cast<AtomicVar<bool>*>(userdata1)->Set(true, MemoryOrder::RELEASE);
}

#pragma endregion Device callbacks

#pragma region WebGPURenderInterface

static constexpr uint32 g_realignBufferSize = 32u * 1024u * 1024u;

WebGPURenderInterface::WebGPURenderInterface()
    : m_renderConfig(MakePimplWithAllocator<WebGPURenderConfig, WebGPUAllocator>()),
      m_instance(nullptr),
      m_adapter(nullptr),
      m_device(nullptr),
      m_queue(nullptr),
      m_deviceLimits(WGPU_LIMITS_INIT),
      m_isDeviceLost(false),
      m_initializeState(InitializeState::NotStarted),
      m_realignBuffer(nullptr),
      m_realignBufferCursor(0),
      m_emptyBindGroup(nullptr)
{
}

WebGPURenderInterface::~WebGPURenderInterface()
{
}

RendererResult WebGPURenderInterface::Initialize()
{
    HYP_LOG(RenderingBackend, Info, "Initializing WebGPU render backend");

    // the config object is built during static initialisation, before the engine config has been read
    m_renderConfig->indirectRendering = g_cvIndirectRendering.Get();

#ifdef HYP_WEB
    g_webGPUDeviceThread = pthread_self();
#endif

    const WGPUInstanceFeatureName instanceFeatures[] = { WGPUInstanceFeatureName_TimedWaitAny };

    WGPUInstanceDescriptor instanceDescriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
    instanceDescriptor.requiredFeatureCount = GetArrayCount(instanceFeatures);
    instanceDescriptor.requiredFeatures = instanceFeatures;

    m_instance = wgpuCreateInstance(&instanceDescriptor);

    if (m_instance == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create WebGPU instance");
    }

    WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

    const ANSIStringView backendName = g_cvWebGPUBackend.Get();

    //https://github.com/HyperionGameTech/HyperionEngine/issues/359
    if (backendName == "d3d12" && backendName.Size() == 5)
    {
        adapterOptions.backendType = WGPUBackendType_D3D12;
    }
    else if (backendName == "vulkan" && backendName.Size() == 6)
    {
        adapterOptions.backendType = WGPUBackendType_Vulkan;
    }

    m_initializeState = InitializeState::WaitingForAdapter;

    WGPURequestAdapterCallbackInfo adapterCallbackInfo = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapterCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    adapterCallbackInfo.callback = &OnAdapterRequestEndedThunk;
    adapterCallbackInfo.userdata1 = this;

    wgpuInstanceRequestAdapter(m_instance, &adapterOptions, adapterCallbackInfo);

    // the web build returns to the event loop here instead and finishes from the device callback
    while (m_initializeState == InitializeState::WaitingForAdapter || m_initializeState == InitializeState::WaitingForDevice)
    {
        WaitForEvents();
    }

    if (m_initializeState != InitializeState::DeviceReady)
    {
        return HYP_MAKE_ERROR(RendererError, "No WebGPU device available");
    }

    return FinishInitialize();
}

void WebGPURenderInterface::OnAdapterRequestEnded(WGPUAdapter adapter)
{
    if (adapter == nullptr)
    {
        m_initializeState = InitializeState::Failed;

        return;
    }

    m_adapter = adapter;

    Array<WGPUFeatureName, WebGPUAllocator> requiredFeatures;

    auto requestFeature = [&](WGPUFeatureName feature) -> bool
    {
        if (!wgpuAdapterHasFeature(m_adapter, feature))
        {
            return false;
        }

        requiredFeatures.PushBack(feature);

        return true;
    };

    m_deviceFeatures.depthClipControl = requestFeature(WGPUFeatureName_DepthClipControl);
    m_deviceFeatures.float32Filterable = requestFeature(WGPUFeatureName_Float32Filterable);
    m_deviceFeatures.depth32FloatStencil8 = requestFeature(WGPUFeatureName_Depth32FloatStencil8);
    m_deviceFeatures.textureFormatsTier1 = requestFeature(WGPUFeatureName_TextureFormatsTier1);
    m_deviceFeatures.indirectFirstInstance = requestFeature(WGPUFeatureName_IndirectFirstInstance);
    m_deviceFeatures.rg11b10UfloatRenderable = requestFeature(WGPUFeatureName_RG11B10UfloatRenderable);

    requestFeature(WGPUFeatureName_TextureFormatsTier2);
    requestFeature(WGPUFeatureName_Float32Blendable);
    requestFeature(WGPUFeatureName_TextureCompressionBC);
#ifndef HYP_WEB
    // Dawn only: lets transient command buffers be recorded off the render thread
    requestFeature(WGPUFeatureName_ImplicitDeviceSynchronization);
#endif

    // ask for everything the adapter has; the web build will need to be held to the default limits instead
    WGPULimits adapterLimits = WGPU_LIMITS_INIT;
    wgpuAdapterGetLimits(m_adapter, &adapterLimits);

    // browsers on D3D12 report 256 for both, so native runs are held to it as well
    adapterLimits.minUniformBufferOffsetAlignment = MathUtil::Max(adapterLimits.minUniformBufferOffsetAlignment, 256u);
    adapterLimits.minStorageBufferOffsetAlignment = MathUtil::Max(adapterLimits.minStorageBufferOffsetAlignment, 256u);

    WGPUDeviceDescriptor deviceDescriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    deviceDescriptor.label = ToWGPUStringView("Hyperion");
    deviceDescriptor.requiredFeatureCount = requiredFeatures.Size();
    deviceDescriptor.requiredFeatures = requiredFeatures.Data();
    deviceDescriptor.requiredLimits = &adapterLimits;
    deviceDescriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceDescriptor.deviceLostCallbackInfo.callback = &OnDeviceLost;
    deviceDescriptor.deviceLostCallbackInfo.userdata1 = const_cast<bool*>(&m_isDeviceLost);
    deviceDescriptor.uncapturedErrorCallbackInfo.callback = &OnUncapturedError;

    m_initializeState = InitializeState::WaitingForDevice;

    WGPURequestDeviceCallbackInfo deviceCallbackInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    deviceCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceCallbackInfo.callback = &OnDeviceRequestEndedThunk;
    deviceCallbackInfo.userdata1 = this;

    wgpuAdapterRequestDevice(m_adapter, &deviceDescriptor, deviceCallbackInfo);
}

void WebGPURenderInterface::OnDeviceRequestEnded(WGPUDevice device)
{
    m_device = device;
    m_initializeState = device != nullptr ? InitializeState::DeviceReady : InitializeState::Failed;
}

RendererResult WebGPURenderInterface::FinishInitialize()
{
    m_queue = wgpuDeviceGetQueue(m_device);

    wgpuDeviceGetLimits(m_device, &m_deviceLimits);

    HYP_LOG(RenderingBackend, Info, "WebGPU limits: uniform offset alignment {}, storage offset alignment {}, dynamic uniform buffers {}, dynamic storage buffers {}, uniform binding size {}, bind groups {}",
        m_deviceLimits.minUniformBufferOffsetAlignment, m_deviceLimits.minStorageBufferOffsetAlignment,
        m_deviceLimits.maxDynamicUniformBuffersPerPipelineLayout, m_deviceLimits.maxDynamicStorageBuffersPerPipelineLayout,
        m_deviceLimits.maxUniformBufferBindingSize, m_deviceLimits.maxBindGroups);

    HYP_LOG(RenderingBackend, Info, "WebGPU features: depth clip control {}, float32 filterable {}, texture formats tier1 {}, rg11b10 renderable {}",
        m_deviceFeatures.depthClipControl, m_deviceFeatures.float32Filterable, m_deviceFeatures.textureFormatsTier1, m_deviceFeatures.rg11b10UfloatRenderable);

    HYP_LOG(RenderingBackend, Info, "WebGPU render config: indirect rendering {}", bool(m_renderConfig->indirectRendering));

    WGPUBufferDescriptor realignBufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    realignBufferDescriptor.label = ToWGPUStringView("RealignedDynamicRanges");
    realignBufferDescriptor.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
    realignBufferDescriptor.size = g_realignBufferSize;

    m_realignBuffer = wgpuDeviceCreateBuffer(m_device, &realignBufferDescriptor);

    for (uint32 frameIndex = 0; frameIndex < uint32(m_frames.Size()); frameIndex++)
    {
        WebGPUFrameRef& frame = m_frames[frameIndex];

        frame = MakeHandle<WebGPUFrame>(frameIndex);
        CheckResultOrReturn(frame->Create());
    }

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_commandBuffers[frameIndex] = MakeHandle<WebGPUCommandBuffer>();
        CheckResultOrReturn(m_commandBuffers[frameIndex]->Create());
    }

    CheckResultOrReturn(RenderInterface::Initialize());

    cbufferAllocator->Initialize(m_deviceLimits.minUniformBufferOffsetAlignment);

    return {};
}

void WebGPURenderInterface::Shutdown()
{
    HYP_LOG(RenderingBackend, Info, "Destroying WebGPU render backend...");

    if (m_device != nullptr)
    {
        WaitForSubmittedWork();
    }

#ifndef HYP_WEB
    m_pipelineManifest.Save();
#endif

    m_pipelineManifest.ReleasePipelines();

    {
        Mutex::Guard guard(m_asyncComputesMutex);

        for (WebGPUAsyncCompute* asyncCompute : m_submittedAsyncComputes)
        {
            delete asyncCompute;
        }

        for (WebGPUAsyncCompute* asyncCompute : m_asyncComputePool)
        {
            delete asyncCompute;
        }

        m_submittedAsyncComputes.Clear();
        m_asyncComputePool.Clear();
    }

    for (WebGPUCommandBufferRef& commandBuffer : m_commandBuffers)
    {
        commandBuffer.Reset();
    }

    for (WebGPUFrameRef& frame : m_frames)
    {
        frame.Reset();
    }

    for (FrameSlot& frameSlot : m_frameSlots)
    {
        frameSlot.readbacks.Clear();
    }

    RenderInterface::Shutdown();

    {
        Mutex::Guard guard(m_transientCommandBuffersMutex);

        AssertDebug(m_recordingTransientCommandBuffers.Empty(), "Transient command buffers are still being recorded at shutdown!");

        m_freeTransientCommandBuffers.Clear();
        m_recordingTransientCommandBuffers.Clear();
        m_transientCommandBufferStorage.Clear();
    }

    {
        Mutex::Guard guard(m_dirtyBuffersMutex);

        m_dirtyBuffers.Clear();
    }

    WebGPUGpuImage::ReleaseSharedResources();
    WebGPUFramebuffer::ReleaseSharedResources();

    {
        Mutex::Guard guard(m_fallbackTexturesMutex);

        for (FallbackTexture& fallbackTexture : m_fallbackTextures)
        {
            wgpuTextureViewRelease(fallbackTexture.view);
            wgpuTextureRelease(fallbackTexture.texture);
        }

        m_fallbackTextures.Clear();
    }

    if (m_emptyBindGroup != nullptr)
    {
        wgpuBindGroupRelease(m_emptyBindGroup);
        m_emptyBindGroup = nullptr;
    }

    {
        Mutex::Guard guard(m_bindGroupLayoutsMutex);

        for (auto& it : m_bindGroupLayouts)
        {
            wgpuBindGroupLayoutRelease(it.second);
        }

        m_bindGroupLayouts.Clear();
        m_storageTextureEntries.Clear();
    }

    if (m_realignBuffer != nullptr)
    {
        wgpuBufferRelease(m_realignBuffer);
        m_realignBuffer = nullptr;
    }

    if (m_queue != nullptr)
    {
        wgpuQueueRelease(m_queue);
        m_queue = nullptr;
    }

    if (m_device != nullptr)
    {
        wgpuDeviceDestroy(m_device);
        wgpuDeviceRelease(m_device);
        m_device = nullptr;
    }

    if (m_adapter != nullptr)
    {
        wgpuAdapterRelease(m_adapter);
        m_adapter = nullptr;
    }

    if (m_instance != nullptr)
    {
        wgpuInstanceRelease(m_instance);
        m_instance = nullptr;
    }
}

const IRenderConfig& WebGPURenderInterface::GetRenderConfig() const
{
    return *m_renderConfig;
}

bool WebGPURenderInterface::CheckDeviceRemoved() const
{
    return m_isDeviceLost;
}

WebGPUFrame* WebGPURenderInterface::GetCurrentFrame() const
{
    return m_frames[GetFrameCounter() % NumFramesInFlight].Get();
}

void WebGPURenderInterface::ProcessEvents()
{
    if (m_instance != nullptr)
    {
        wgpuInstanceProcessEvents(m_instance);
    }

#ifdef HYP_WEB
    if (IsOnWebGPUDeviceThread())
    {
        // calls other threads have handed over, which otherwise wait for this thread's next yield
        emscripten_current_thread_process_queued_calls();
    }
#endif
}

void WebGPURenderInterface::WaitForEvents()
{
#ifdef HYP_WEB
    if (!IsOnWebGPUDeviceThread())
    {
        ThreadSleep(1);

        return;
    }

    // futures are promises here, they only resolve once this thread is back in its event loop
    emscripten_sleep(0);
    ProcessEvents();
#else
    ProcessEvents();
    ThreadSleep(0);
#endif
}

bool WebGPURenderInterface::IsFrameSlotComplete(const FrameSlot& frameSlot) const
{
    if (!frameSlot.isWorkDone.Get(MemoryOrder::ACQUIRE))
    {
        return false;
    }

    for (const WebGPUGpuBufferRef& readback : frameSlot.readbacks)
    {
        if (readback->IsReadbackPending())
        {
            return false;
        }
    }

    return true;
}

void WebGPURenderInterface::WaitForFrameSlot(FrameSlot& frameSlot)
{
    ProcessEvents();

    // The one place a frame stalls on the GPU, the counterpart of waiting on the frame fence in the other backends.
    // A browser cannot spin here; the web build skips the tick and tries the slot again on the next one.
    while (!IsFrameSlotComplete(frameSlot) && !m_isDeviceLost)
    {
        WaitForEvents();
    }

    frameSlot.readbacks.Clear();
}

void WebGPURenderInterface::WaitForSubmittedWork()
{
#ifdef HYP_WEB
    // WaitAny suspends, which only the device thread may do; anyone else polls while that thread runs its events
    AtomicVar<bool> isWorkDone { false };

    WGPUQueueWorkDoneCallbackInfo workDoneCallbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    workDoneCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    workDoneCallbackInfo.callback = &OnFrameWorkDone;
    workDoneCallbackInfo.userdata1 = &isWorkDone;

    wgpuQueueOnSubmittedWorkDone(m_queue, workDoneCallbackInfo);

    while (!isWorkDone.Get(MemoryOrder::ACQUIRE) && !m_isDeviceLost)
    {
        WaitForEvents();
    }

    return;
#endif

    bool isDone = false;

    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = &OnSubmittedWorkDone;
    callbackInfo.userdata1 = &isDone;

    WGPUFutureWaitInfo waitInfo {};
    waitInfo.future = wgpuQueueOnSubmittedWorkDone(m_queue, callbackInfo);

    while (!isDone && !m_isDeviceLost)
    {
        if (wgpuInstanceWaitAny(m_instance, 1, &waitInfo, UINT64_MAX) != WGPUWaitStatus_Success)
        {
            break;
        }
    }
}

bool WebGPURenderInterface::AllocateRealignedRange(uint32 size, uint32& outOffset)
{
    const uint32 alignedSize = ByteUtil::AlignAs(size, m_deviceLimits.minStorageBufferOffsetAlignment);

    Mutex::Guard guard(m_realignBufferMutex);

    if (uint64(m_realignBufferCursor) + alignedSize > g_realignBufferSize)
    {
        HYP_LOG_ONCE(RenderingBackend, Error, "Out of space for realigned dynamic buffer ranges ({} bytes)", g_realignBufferSize);

        return false;
    }

    outOffset = m_realignBufferCursor;
    m_realignBufferCursor += alignedSize;

    return true;
}

void WebGPURenderInterface::PrepareFrame(WebGPUFrame* frame)
{
    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    {
        // queue writes are ordered with submits, so the slots of the previous frame can be handed out again
        Mutex::Guard guard(m_realignBufferMutex);

        m_realignBufferCursor = 0;
    }

    m_pipelineManifest.Update();
    m_pipelineCompiler.Update();

    // the handlers bound to OnFrameEnd read back what the frame copied out, so that has to have landed first
    WaitForFrameSlot(m_frameSlots[frameIndex]);

    stagingBufferPool->ReleaseForCommandBuffer(m_commandBuffers[frameIndex].Get());

    if (frame->OnFrameEnd.AnyBound())
    {
        frame->OnFrameEnd(frame);
        frame->OnFrameEnd.RemoveAllDetached();
    }

    for (auto it = m_submittedAsyncComputes.Begin(); it != m_submittedAsyncComputes.End();)
    {
        WebGPUAsyncCompute* asyncCompute = *it;

        if (asyncCompute->CheckStatus())
        {
            asyncCompute->OnCompleted();
            asyncCompute->OnCompleted.RemoveAllDetached();

            m_asyncComputePool.PushBack(asyncCompute);

            it = m_submittedAsyncComputes.Erase(it);

            continue;
        }

        ++it;
    }

    frame->OnFrameStart();
}

WebGPUSwapchainRef WebGPURenderInterface::CreateSwapchain(ApplicationWindow* window, const Vec2u& extent)
{
    Assert(window != nullptr);

    WebGPUSwapchainRef swapchain = MakeHandle<WebGPUSwapchain>(window, extent);

    RendererResult result = swapchain->Create();

    if (!result)
    {
        HYP_FAIL("Failed to create WebGPU swapchain: {}", result.GetError().GetMessage());
    }

    return swapchain;
}

void WebGPURenderInterface::PrepareSwapchain(WebGPUSwapchain* swapchain)
{
    swapchain->PrepareForFrame();
}

void WebGPURenderInterface::PresentToSwapchain(WebGPUSwapchain* swapchain)
{
    WebGPUCommandBuffer* commandBuffer = GetCurrentCommandBuffer();
    AssertDebug(commandBuffer != nullptr);
    AssertDebug(!commandBuffer->IsRecording());

    FrameSlot& frameSlot = m_frameSlots[GetFrameCounter() % NumFramesInFlight];

    for (WebGPUGpuBuffer* readback : commandBuffer->GetPendingReadbacks())
    {
        frameSlot.readbacks.PushBack(MakeStrongRef(readback));
    }

    Submit(*commandBuffer);

    frameSlot.isWorkDone.Set(false, MemoryOrder::RELEASE);

    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = &OnFrameWorkDone;
    callbackInfo.userdata1 = &frameSlot.isWorkDone;

    wgpuQueueOnSubmittedWorkDone(m_queue, callbackInfo);

    if (swapchain != nullptr)
    {
        swapchain->PresentFrame();
    }
}

void WebGPURenderInterface::RegisterDirtyBuffer(WebGPUGpuBuffer* buffer)
{
    Mutex::Guard guard(m_dirtyBuffersMutex);

    m_dirtyBuffers.PushBack(buffer);
}

void WebGPURenderInterface::UnregisterDirtyBuffer(WebGPUGpuBuffer* buffer)
{
    Mutex::Guard guard(m_dirtyBuffersMutex);

    auto it = m_dirtyBuffers.Find(buffer);

    if (it != m_dirtyBuffers.End())
    {
        m_dirtyBuffers.Erase(it);
    }
}

void WebGPURenderInterface::UploadDirtyBuffers()
{
    Mutex::Guard guard(m_dirtyBuffersMutex);

    for (WebGPUGpuBuffer* buffer : m_dirtyBuffers)
    {
        buffer->UploadDirtyRange(m_queue);
    }

    m_dirtyBuffers.Clear();
}

void WebGPURenderInterface::Submit(WebGPUCommandBuffer& commandBuffer)
{
    const Span<const WGPUCommandBuffer> finishedCommandBuffers = commandBuffer.GetFinishedCommandBuffers();

    if (finishedCommandBuffers.Size() == 0)
    {
        return;
    }

    {
        Mutex::Guard guard(m_submitMutex);

        UploadDirtyBuffers();

        wgpuQueueSubmit(m_queue, finishedCommandBuffers.Size(), finishedCommandBuffers.Data());
    }

    commandBuffer.ReleaseFinishedCommandBuffers();
    commandBuffer.OnSubmitted();
}

WebGPUCommandBuffer& WebGPURenderInterface::GetTransientCommandBuffer()
{
    WebGPUTransientCommandBuffer* transientCommandBuffer = nullptr;

    {
        Mutex::Guard guard(m_transientCommandBuffersMutex);

        if (m_freeTransientCommandBuffers.Any())
        {
            transientCommandBuffer = m_freeTransientCommandBuffers.PopBack();
        }
        else
        {
            transientCommandBuffer = &m_transientCommandBufferStorage.EmplaceBack();
        }

        m_recordingTransientCommandBuffers.PushBack(transientCommandBuffer);
    }

    WebGPUCommandBuffer& commandBuffer = transientCommandBuffer->commandBuffer;
    commandBuffer.Begin();

    return commandBuffer;
}

void WebGPURenderInterface::SubmitTransientCommandBuffer(WebGPUCommandBuffer& commandBuffer)
{
    if (commandBuffer.IsRecording())
    {
        commandBuffer.End();
    }

    WebGPUTransientCommandBuffer* transientCommandBuffer = nullptr;

    {
        Mutex::Guard guard(m_transientCommandBuffersMutex);

        for (auto it = m_recordingTransientCommandBuffers.Begin(); it != m_recordingTransientCommandBuffers.End(); ++it)
        {
            if (&(*it)->commandBuffer == &commandBuffer)
            {
                transientCommandBuffer = *it;
                m_recordingTransientCommandBuffers.Erase(it);

                break;
            }
        }
    }

    Assert(transientCommandBuffer != nullptr, "Command buffer was not acquired via GetTransientCommandBuffer()");

    Submit(commandBuffer);

    if (stagingBufferPool != nullptr)
    {
        stagingBufferPool->ReleaseForCommandBuffer(&commandBuffer);
    }

    Mutex::Guard guard(m_transientCommandBuffersMutex);

    m_freeTransientCommandBuffers.PushBack(transientCommandBuffer);
}

WGPUBindGroupLayout WebGPURenderInterface::GetOrCreateBindGroupLayout(const WGPUBindGroupLayoutEntry* entries, uint32 numEntries)
{
    HashCode hashCode;
    hashCode.Add(numEntries);

    for (uint32 entryIndex = 0; entryIndex < numEntries; entryIndex++)
    {
        const WGPUBindGroupLayoutEntry& entry = entries[entryIndex];

        hashCode.Add(entry.binding);
        hashCode.Add(uint64(entry.visibility));
        hashCode.Add(uint32(entry.buffer.type));
        hashCode.Add(uint32(entry.buffer.hasDynamicOffset));
        hashCode.Add(uint32(entry.sampler.type));
        hashCode.Add(uint32(entry.texture.sampleType));
        hashCode.Add(uint32(entry.texture.viewDimension));
        hashCode.Add(uint32(entry.storageTexture.access));
        hashCode.Add(uint32(entry.storageTexture.format));
        hashCode.Add(uint32(entry.storageTexture.viewDimension));
    }

    const uint64 key = hashCode.Value();

    Mutex::Guard guard(m_bindGroupLayoutsMutex);

    auto it = m_bindGroupLayouts.Find(key);

    if (it != m_bindGroupLayouts.End())
    {
        return it->second;
    }

    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.entryCount = numEntries;
    descriptor.entries = entries;

    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(m_device, &descriptor);

    m_bindGroupLayouts.Insert(key, layout);

#ifndef HYP_WEB
    m_bindGroupLayoutEntries.Insert(layout, Array<WGPUBindGroupLayoutEntry, WebGPUAllocator>(Span<const WGPUBindGroupLayoutEntry>(entries, numEntries)));
#endif

    Array<WebGPUStorageTextureOverride, WebGPUAllocator> storageTextureEntries;

    for (uint32 entryIndex = 0; entryIndex < numEntries; entryIndex++)
    {
        const WGPUBindGroupLayoutEntry& entry = entries[entryIndex];

        if (entry.storageTexture.format != WGPUTextureFormat_Undefined)
        {
            storageTextureEntries.PushBack(WebGPUStorageTextureOverride { 0, entry.binding, entry.storageTexture.format, entry.storageTexture.access });
        }
    }

    if (storageTextureEntries.Any())
    {
        m_storageTextureEntries.Insert(layout, std::move(storageTextureEntries));
    }

    return layout;
}

void WebGPURenderInterface::GetStorageTextureOverrides(WGPUBindGroupLayout layout, uint32 group, Array<WebGPUStorageTextureOverride, WebGPUAllocator>& outOverrides)
{
    Mutex::Guard guard(m_bindGroupLayoutsMutex);

    auto it = m_storageTextureEntries.Find(layout);

    if (it == m_storageTextureEntries.End())
    {
        return;
    }

    for (const WebGPUStorageTextureOverride& storageTextureEntry : it->second)
    {
        WebGPUStorageTextureOverride storageOverride = storageTextureEntry;
        storageOverride.group = group;

        outOverrides.PushBack(storageOverride);
    }
}

bool WebGPURenderInterface::GetBindGroupLayoutEntries(WGPUBindGroupLayout layout, Array<WGPUBindGroupLayoutEntry, WebGPUAllocator>& outEntries)
{
    Mutex::Guard guard(m_bindGroupLayoutsMutex);

    auto it = m_bindGroupLayoutEntries.Find(layout);

    if (it == m_bindGroupLayoutEntries.End())
    {
        return false;
    }

    outEntries = it->second;

    return true;
}

WGPUBindGroupLayout WebGPURenderInterface::GetEmptyBindGroupLayout()
{
    return GetOrCreateBindGroupLayout(nullptr, 0);
}

WGPUBindGroup WebGPURenderInterface::GetEmptyBindGroup()
{
    WGPUBindGroupLayout layout = GetEmptyBindGroupLayout();

    Mutex::Guard guard(m_bindGroupLayoutsMutex);

    if (m_emptyBindGroup == nullptr)
    {
        WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        descriptor.layout = layout;

        m_emptyBindGroup = wgpuDeviceCreateBindGroup(m_device, &descriptor);
    }

    return m_emptyBindGroup;
}

WGPUTextureView WebGPURenderInterface::GetFallbackTextureView(WGPUTextureViewDimension dimension, WGPUTextureSampleType sampleType)
{
    Mutex::Guard guard(m_fallbackTexturesMutex);

    for (const FallbackTexture& fallbackTexture : m_fallbackTextures)
    {
        if (fallbackTexture.dimension == dimension && fallbackTexture.sampleType == sampleType)
        {
            return fallbackTexture.view;
        }
    }

    WGPUTextureFormat format;

    switch (sampleType)
    {
    case WGPUTextureSampleType_Uint:
        format = WGPUTextureFormat_R32Uint;
        break;
    case WGPUTextureSampleType_Sint:
        format = WGPUTextureFormat_R32Sint;
        break;
    case WGPUTextureSampleType_Depth:
        format = WGPUTextureFormat_Depth32Float;
        break;
    default:
        format = WGPUTextureFormat_RGBA8Unorm;
        break;
    }

    const bool isCube = dimension == WGPUTextureViewDimension_Cube || dimension == WGPUTextureViewDimension_CubeArray;
    const bool isVolume = dimension == WGPUTextureViewDimension_3D;

    WGPUTextureDescriptor textureDescriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    textureDescriptor.usage = WGPUTextureUsage_TextureBinding;
    textureDescriptor.dimension = isVolume ? WGPUTextureDimension_3D : WGPUTextureDimension_2D;
    textureDescriptor.size.width = 1;
    textureDescriptor.size.height = 1;
    textureDescriptor.size.depthOrArrayLayers = isCube ? 6 : 1;
    textureDescriptor.format = format;
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.sampleCount = 1;

    WGPUTexture texture = wgpuDeviceCreateTexture(m_device, &textureDescriptor);

    WGPUTextureViewDescriptor viewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    viewDescriptor.format = format;
    viewDescriptor.dimension = dimension;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.arrayLayerCount = isCube ? 6 : 1;
    viewDescriptor.aspect = sampleType == WGPUTextureSampleType_Depth ? WGPUTextureAspect_DepthOnly : WGPUTextureAspect_All;

    WGPUTextureView view = wgpuTextureCreateView(texture, &viewDescriptor);

    m_fallbackTextures.PushBack(FallbackTexture { dimension, sampleType, texture, view });

    return view;
}

WebGPUDescriptorSetRef WebGPURenderInterface::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    return MakeHandle<WebGPUDescriptorSet>(layout);
}

WebGPUDescriptorTableRef WebGPURenderInterface::MakeDescriptorTable(const ShaderInputGroup* decl)
{
    return MakeHandle<WebGPUDescriptorTable>(decl);
}

WebGPUGraphicsPipelineRef WebGPURenderInterface::MakeGraphicsPipeline(
    const WebGPUShaderInstanceRef& shaderInstance,
    const FramebufferDesc& framebufferDesc,
    const RenderableAttributeSet& attributes,
    uint8 stencilWriteMask,
    uint8 stencilCompareMask)
{
    WebGPUGraphicsPipelineRef pipeline = MakeHandle<WebGPUGraphicsPipeline>();

    if (shaderInstance.IsValid())
    {
        pipeline->SetShader(shaderInstance);

#ifdef HYP_RHI_DEBUG_NAMES
        pipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shaderInstance->GetDebugName().IsValid() ? *shaderInstance->GetDebugName() : "<unnamed shader>"));
#endif
    }

    pipeline->SetFramebufferDesc(framebufferDesc);
    pipeline->SetInputLayout(attributes.GetMeshAttributes().inputLayout);
    pipeline->SetTopology(attributes.GetMeshAttributes().topology);
    pipeline->SetCullMode(attributes.GetMaterialAttributes().cullFaces);
    pipeline->SetFillMode(attributes.GetMaterialAttributes().fillMode);
    pipeline->SetBlendFunction(attributes.GetMaterialAttributes().blendFunction);
    pipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    pipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    pipeline->SetDepthCompareOp(attributes.GetMaterialAttributes().depthCompareOp);
    pipeline->SetDepthClamp(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP));

    if (attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
    {
        pipeline->SetDepthBias(attributes.GetMaterialAttributes().depthBias);
        pipeline->SetDepthBiasSlope(attributes.GetMaterialAttributes().depthBiasSlope);
    }

    if (attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)
    {
        pipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);
        pipeline->SetStencilWriteMask(stencilWriteMask);
        pipeline->SetStencilCompareMask(stencilCompareMask);
    }

    if (attributes.GetMaterialAttributes().stencilReference != 0)
    {
        pipeline->SetStencilWrite(true);
    }

    AssertDebug(pipeline->MatchesSignature(attributes, framebufferDesc, stencilWriteMask, stencilCompareMask));

    return pipeline;
}

WebGPUComputePipelineRef WebGPURenderInterface::MakeComputePipeline(const WebGPUShaderInstanceRef& shaderInstance)
{
    return MakeHandle<WebGPUComputePipeline>(shaderInstance);
}

WebGPURayTracingPipelineRef WebGPURenderInterface::MakeRayTracingPipeline(const WebGPUShaderInstanceRef& shaderInstance)
{
    return MakeHandle<WebGPURayTracingPipeline>(shaderInstance);
}

WebGPUGpuBufferRef WebGPURenderInterface::MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment)
{
    return MakeHandle<WebGPUGpuBuffer>(bufferType, size, alignment);
}

WebGPUGpuImageRef WebGPURenderInterface::MakeImage(const TextureDesc& textureDesc)
{
    return MakeHandle<WebGPUGpuImage>(textureDesc);
}

WebGPUGpuImageViewRef WebGPURenderInterface::MakeImageView(const WebGPUGpuImageRef& image)
{
    return MakeHandle<WebGPUGpuImageView>(image);
}

WebGPUGpuImageViewRef WebGPURenderInterface::MakeImageView(
    const WebGPUGpuImageRef& image,
    uint8 mipIndex,
    uint8 numMips,
    uint16 layerIndex,
    uint16 numLayers,
    TextureType viewType)
{
    ImageSubResource subResource {};
    subResource.baseMipLevel = mipIndex;
    subResource.baseArrayLayer = layerIndex;
    subResource.numLevels = numMips;
    subResource.numLayers = numLayers;

    return MakeHandle<WebGPUGpuImageView>(image, subResource, viewType);
}

WebGPUSamplerRef WebGPURenderInterface::MakeSampler(const SamplerDesc& samplerDesc)
{
    return MakeHandle<WebGPUSampler>(samplerDesc);
}

WebGPUFramebufferRef WebGPURenderInterface::MakeFramebuffer(const FramebufferDesc& framebufferDesc)
{
    return MakeHandle<WebGPUFramebuffer>(framebufferDesc);
}

WebGPUFrameRef WebGPURenderInterface::MakeFrame(uint32 frameIndex)
{
    return MakeHandle<WebGPUFrame>(frameIndex);
}

WebGPUShaderInstanceRef WebGPURenderInterface::MakeShader(const Shader* shader)
{
    return MakeHandle<WebGPUShaderInstance>(shader);
}

WebGPUBottomLevelASRef WebGPURenderInterface::MakeBottomLevelAS(
    const WebGPUGpuBufferRef& packedVerticesBuffer,
    const WebGPUGpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
{
    return MakeHandle<WebGPUBottomLevelAS>();
}

WebGPUTopLevelASRef WebGPURenderInterface::MakeTLAS()
{
    ASResourceCallbacks callbacks {};

    callbacks.setBLASBuffers = [](uint64 key, WebGPUGpuBuffer* vb, WebGPUGpuBuffer* ib) -> uint32
    {
        return BLASCache::InvalidStorageId;
    };

    callbacks.removeBLASBuffers = [](uint64 key) -> bool
    {
        return false;
    };

    return MakeHandle<WebGPUTopLevelAS>(callbacks);
}

void WebGPURenderInterface::PopulateIndirectDrawCommandsBuffer(
    const WebGPUGpuBuffer* vertexBuffer,
    const WebGPUGpuBuffer* indexBuffer,
    uint32 numIndices,
    uint32 instanceOffset,
    Array<IndirectDrawCommand, WebGPUAllocator>& outBuffer)
{
    const size_t requiredSize = size_t(instanceOffset) + 1;

    if (outBuffer.Size() < requiredSize)
    {
        outBuffer.ResizeUninitialized(requiredSize);
    }

    IndirectDrawCommand& command = outBuffer[instanceOffset];
    command = IndirectDrawCommand {};
    command.IndexCountPerInstance = indexBuffer != nullptr ? numIndices : 0;
    command.InstanceCount = 0;
    command.StartIndexLocation = 0;
    command.BaseVertexLocation = 0;
    command.StartInstanceLocation = 0;
}

bool WebGPURenderInterface::IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
{
    const WGPUTextureFormat wgpuFormat = ToWGPUTextureFormat(format);

    if (wgpuFormat == WGPUTextureFormat_Undefined)
    {
        return false;
    }

    switch (supportType)
    {
    case ImageSupport::ShaderResource:
        return true;
    case ImageSupport::Attachment:
        if (wgpuFormat == WGPUTextureFormat_RG11B10Ufloat)
        {
            return m_deviceFeatures.rg11b10UfloatRenderable;
        }

        return wgpuFormat != WGPUTextureFormat_RGB9E5Ufloat;
    case ImageSupport::UnorderedAccess:
        switch (wgpuFormat)
        {
        case WGPUTextureFormat_RGBA8Unorm:
        case WGPUTextureFormat_RGBA8Snorm:
        case WGPUTextureFormat_RGBA8Uint:
        case WGPUTextureFormat_RGBA8Sint:
        case WGPUTextureFormat_RGBA16Uint:
        case WGPUTextureFormat_RGBA16Sint:
        case WGPUTextureFormat_RGBA16Float:
        case WGPUTextureFormat_R32Float:
        case WGPUTextureFormat_R32Uint:
        case WGPUTextureFormat_R32Sint:
        case WGPUTextureFormat_RG32Float:
        case WGPUTextureFormat_RG32Uint:
        case WGPUTextureFormat_RG32Sint:
        case WGPUTextureFormat_RGBA32Float:
        case WGPUTextureFormat_RGBA32Uint:
        case WGPUTextureFormat_RGBA32Sint:
            return true;
        default:
            return m_deviceFeatures.textureFormatsTier1 && !TextureUtils::IsDepthFormat(format);
        }
    default:
        return false;
    }
}

TextureFormat WebGPURenderInterface::FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
{
    for (TextureFormat format : possibleFormats)
    {
        if (IsSupportedFormat(format, supportType))
        {
            return format;
        }
    }

    return InvalidTextureFormat;
}

UniquePtr<SingleTimeCommands> WebGPURenderInterface::GetSingleTimeCommands()
{
    return MakeUnique<WebGPUSingleTimeCommands>();
}

WebGPUAsyncCompute* WebGPURenderInterface::CreateAsyncCompute()
{
    {
        Mutex::Guard guard(m_asyncComputesMutex);

        if (m_asyncComputePool.Any())
        {
            return m_asyncComputePool.PopBack();
        }
    }

    WebGPUAsyncCompute* asyncCompute = new WebGPUAsyncCompute();
    asyncCompute->Create();

    return asyncCompute;
}

void WebGPURenderInterface::SubmitAsyncCompute(WebGPUAsyncCompute* asyncCompute)
{
    Assert(asyncCompute != nullptr);

    Mutex::Guard guard(m_asyncComputesMutex);

    Assert(!m_submittedAsyncComputes.Contains(asyncCompute));

    asyncCompute->Submit();

    m_submittedAsyncComputes.PushBack(asyncCompute);
}

void WebGPURenderInterface::RecordStartTimestamp(WebGPUCommandBuffer* cmd, EngineStatGpuTimer* timer)
{
}

void WebGPURenderInterface::RecordStopTimestamp(WebGPUCommandBuffer* cmd, EngineStatGpuTimer* timer)
{
}

void WebGPURenderInterface::ResolveGpuFrameResults(uint32 completedFrameIndex)
{
}

void WebGPURenderInterface::ReleaseTransientMemory()
{
    GetCurrentFrame()->ResetTransientStates();
}

void WebGPURenderInterface::InitDeviceDetails(DeviceDetails& deviceDetails)
{
    WGPUAdapterInfo adapterInfo = WGPU_ADAPTER_INFO_INIT;
    wgpuAdapterGetInfo(m_adapter, &adapterInfo);

    const bool isIntegrated = adapterInfo.adapterType != WGPUAdapterType_DiscreteGPU;

    GpuInfo info;
    info.gpuType = isIntegrated ? GpuType::Integrated : GpuType::Dedicated;
    info.vendorId = adapterInfo.vendorID;
    info.deviceId = adapterInfo.deviceID;
    info.gpuModel = String(ToStringView(adapterInfo.device));
    info.isDiscrete = !isIntegrated;
    info.supportsRayTracing = false;

    deviceDetails.Set(info);

    wgpuAdapterInfoFreeMembers(adapterInfo);
}

#pragma endregion WebGPURenderInterface

} // namespace Hyperion
