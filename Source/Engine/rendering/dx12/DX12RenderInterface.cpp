/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12Swapchain.hpp>
#include <rendering/dx12/DX12AsyncCompute.hpp>
#include <rendering/dx12/DX12GpuTimerBackend.hpp>
#include <rendering/dx12/DX12Fence.hpp>
#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12ComputePipeline.hpp>
#include <rendering/dx12/DX12RayTracingPipeline.hpp>
#include <rendering/dx12/DX12ShaderInstance.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/CrashHandler.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/CBufferAllocator.hpp>

#include <Core/threading/AtomicFlag.hpp>

#include <engine/DeviceDetails.hpp>

#include <system/AppContext.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/utilities/Optional.hpp>

#include <engine/config/EngineConfig.hpp>

#include <dxgi1_6.h>

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
#include <Aftermath/GFSDK_Aftermath.h>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern EngineStatGpuTimer g_statGpuFrameTime;

#define HYP_DX12_ENABLE_DEBUG_LAYER
// #define HYP_DX12_ENABLE_DRED

#pragma region DX12RenderConfig

class DX12RenderConfig final : public IRenderConfig
{
public:
    DX12RenderConfig()
    {
        EngineConfig cfg;
        cfg.Load();

        bindlessTextures = false;
        rayTracing = false;
        indirectRendering = cfg.Get("Rendering.IndirectRendering").ToBool(/* defaultValue */ true);
        parallelRendering = cfg.Get("Rendering.ParallelCollection").ToBool(/* defaultValue */ true);
        dynamicDescriptorIndexing = true;
    }

    void InitializeBindless(DX12RenderInterface* renderInterface)
    {
        ID3D12Device* device = renderInterface->GetDevice();

        D3D12_FEATURE_DATA_D3D12_OPTIONS options {};
        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));

        if (SUCCEEDED(hr))
        {
            // We only use bindless for SRVs (textures, buffers) -- we currently aren't using bindless for UAVs or CBVs.
            // Tier 2 resource binding support provides full heap access for SRVs.
            bindlessTextures = (options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_2);
        }
    }

    void InitializeRayTracing(DX12RenderInterface* renderInterface)
    {
        ID3D12Device* device = renderInterface->GetDevice();

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 {};
        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

        if (SUCCEEDED(hr))
        {
            rayTracing = (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
        }

        if (rayTracing)
        {
            HYP_LOG(RenderingBackend, Info, "Ray tracing is supported (tier {})", int(options5.RaytracingTier));
        }
    }
};

#pragma endregion DX12RenderConfig

#pragma region DX12SingleTimeCommands

class DX12SingleTimeCommands final : public SingleTimeCommands
{
public:
    DX12SingleTimeCommands() = default;

    virtual ~DX12SingleTimeCommands() override = default;

    virtual RendererResult Execute() override
    {
        AssertOnThread(g_renderThread);

        CommandRecorder cr;

        for (auto& fn : m_functions)
        {
            fn(cr);
        }

        m_functions.Clear();

        DX12Frame tempFrame;
        CheckResultOrReturn(tempFrame.Create());

        cr.Prepare(&tempFrame);

        DX12Fence fence;
        CheckResultOrReturn(fence.Create());

        DX12CommandBuffer commandBuffer(D3D12_COMMAND_LIST_TYPE_DIRECT);
        CheckResultOrReturn(commandBuffer.Create());

        commandBuffer.Begin();
        cr.Execute(&commandBuffer);
        commandBuffer.End();

        const DX12QueueData* queueData = RI.GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
        Assert(queueData != nullptr && queueData->commandQueue != nullptr);

        ID3D12CommandList* commandLists[] = { commandBuffer.GetCommandList() };
        queueData->commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);

        fence.Increment();

        HRESULT hr = queueData->commandQueue->Signal(fence.GetD3D12Fence(), fence.GetValue());
        Assert(SUCCEEDED(hr));

        return fence.Wait();
    }
};

#pragma endregion DX12SingleTimeCommands

#pragma region DX12RenderInterface

DX12RenderInterface::DX12RenderInterface()
    : descriptorHeapManager(nullptr),
      m_allocator(nullptr),
      m_frameFenceEvent(nullptr),
      m_frameFenceIndex(0)
{
    // Initialize fence values to 0 (no submissions yet)
    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        m_frameFenceValues[i] = 1;
        m_submissionFrames[i] = -1;
    }
}

DX12RenderInterface::~DX12RenderInterface()
{
}

RendererResult DX12RenderInterface::Initialize()
{
    HYP_LOG(RenderingBackend, Info, "Initializing DX12 render backend");

    descriptorHeapManager = PoolNew<DX12DescriptorHeapManager>(*g_renderPool);
    m_gpuTimerBackend = PoolNew<DX12GpuTimerBackend>(*g_renderPool);
    m_renderConfig = MakePimpl<DX12RenderConfig>();

    uint32 createFactoryFlags = 0;

#ifdef HYP_DEBUG_MODE
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT res = CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), &dxgiFactory);
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create DXGI Factory", res);

    EngineConfig cfg;
    cfg.Load();

    const ConfigValue& cfgSelectedGpuIndex = cfg.Get("System.SelectedGpu.Index");

    bool gpuSelected = false;
    int targetGpuIndex = -1;

    if (cfgSelectedGpuIndex.IsNumber())
    {
        targetGpuIndex = cfgSelectedGpuIndex.ToInt32();
    }

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(dxgiFactory.As(&factory6)))
    {
        UINT validAdapterIndex = 0;

        for (UINT i = 0; SUCCEEDED(factory6->EnumAdapters1(i, &m_hardwareAdapter)); ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            m_hardwareAdapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
            {
                bool selectThisAdapter = false;

                if (validAdapterIndex == targetGpuIndex)
                {
                    selectThisAdapter = true;
                }
                else if (targetGpuIndex < 0 && !gpuSelected)
                {
                    selectThisAdapter = true;
                }

                if (selectThisAdapter)
                {
                    HYP_LOG(RenderingBackend, Info, "Selected GPU index {}: {}", validAdapterIndex, WideString(desc.Description));

                    if (targetGpuIndex < 0)
                    {
                        cfg.Set("System.SelectedGpu.Index", JSON::Number(validAdapterIndex));

                        if (!cfg.Save())
                        {
                           HYP_LOG(RenderingBackend, Warning, "Failed to save GPU selection config");
                        }
                    }

                    gpuSelected = true;
                }

                if (gpuSelected)
                    break;

                validAdapterIndex++;
            }
        }
    }
    else
    {
        UINT validAdapterIndex = 0;

        for (UINT i = 0; SUCCEEDED(dxgiFactory->EnumAdapters1(i, &m_hardwareAdapter)); ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            m_hardwareAdapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
            {
                bool selectThisAdapter = false;

                if (validAdapterIndex == targetGpuIndex)
                {
                    selectThisAdapter = true;
                }
                else if (targetGpuIndex < 0 && !gpuSelected)
                {
                    selectThisAdapter = true;
                }

                if (selectThisAdapter)
                {
                    HYP_LOG(RenderingBackend, Info, "Selected GPU index {}: {}", validAdapterIndex, WideString(desc.Description));

                    if (targetGpuIndex < 0)
                    {
                        cfg.Set("System.SelectedGpu.Index", JSON::Number(validAdapterIndex));
                        if (!cfg.Save())
                        {
                            HYP_LOG(RenderingBackend, Warning, "Failed to save GPU selection config");
                        }
                    }

                    gpuSelected = true;
                }

                if (gpuSelected)
                    break;

                validAdapterIndex++;
            }
        }
    }

    if (!gpuSelected)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to find a suitable GPU adapter");
        return HYP_MAKE_ERROR(RendererError, "Failed to find suitable GPU", E_FAIL);
    }

#ifdef HYP_DEBUG_MODE
#ifdef HYP_DX12_ENABLE_DRED
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings), &m_dredSettings)))
    {
        m_dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        m_dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
#endif

#ifdef HYP_DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController)))
    {
        debugController->EnableDebugLayer();
    }
#endif
#endif

    // CrashHandler must be initialized before we create the device
    crashHandler = PoolNew<CrashHandler>(*g_renderPool);
    crashHandler->Initialize();

    // create device
    res = D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D device!", res);

#ifdef HYP_DEBUG_MODE
    m_device->SetName(L"D3D12 Device");
#endif

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    GFSDK_Aftermath_DX12_Initialize(
        GFSDK_Aftermath_Version_API,
        GFSDK_Aftermath_FeatureFlags_EnableResourceTracking |
            GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo |
            GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting,
        m_device.Get());
#endif

    // Initialize render config features based on device capabilities
    static_cast<DX12RenderConfig*>(m_renderConfig.Get())->InitializeBindless(this);
    static_cast<DX12RenderConfig*>(m_renderConfig.Get())->InitializeRayTracing(this);

    static_assert(sizeof(decltype(m_queueData)) / sizeof(decltype(m_queueData[0])) > D3D12_COMMAND_LIST_TYPE_COPY,
        "m_queueData is too small; must have size increased.");

    Memory::Zero(&m_queueData, sizeof(m_queueData));

    // create queues
    D3D12_COMMAND_QUEUE_DESC directDesc {};
    directDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    directDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    DX12QueueData& directQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT];
    m_device->CreateCommandQueue(&directDesc, __uuidof(ID3D12CommandQueue), &directQueueData.commandQueue);
#ifdef HYP_DEBUG_MODE
    directQueueData.commandQueue->SetName(L"D3D12 Direct Command Queue");
#endif

    D3D12_COMMAND_QUEUE_DESC computeDesc {};
    computeDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    DX12QueueData& computeQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_COMPUTE];
    m_device->CreateCommandQueue(&computeDesc, __uuidof(ID3D12CommandQueue), &computeQueueData.commandQueue);
#ifdef HYP_DEBUG_MODE
    computeQueueData.commandQueue->SetName(L"D3D12 Compute Command Queue");
#endif

    D3D12_COMMAND_QUEUE_DESC copyDesc {};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

    DX12QueueData& copyQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_COPY];
    m_device->CreateCommandQueue(&copyDesc, __uuidof(ID3D12CommandQueue), &copyQueueData.commandQueue);
#ifdef HYP_DEBUG_MODE
    copyQueueData.commandQueue->SetName(L"D3D12 Copy Command Queue");
#endif

    D3D12MA::ALLOCATOR_DESC allocatorDesc {};
    allocatorDesc.pDevice = m_device.Get();
    allocatorDesc.pAdapter = m_hardwareAdapter.Get();

    res = D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D12MemoryAllocator instance!", res);

    // create frames
    for (uint32 frameIndex = 0; frameIndex < uint32(m_frames.Size()); frameIndex++)
    {
        DX12FrameRef& frame = m_frames[frameIndex];

        frame = MakeHandle<DX12Frame>(frameIndex);
        CheckResultOrReturn(frame->Create());
    }

    // create frame synchronization fence (single fence with per-frame values)
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFence));
    if (FAILED(hr))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create frame fence", hr);
    }

#ifdef HYP_DEBUG_MODE
    m_frameFence->SetName(L"D3D12 Frame Fence");
#endif

    m_frameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_frameFenceEvent == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create frame fence event");
    }

    // Create transient sync fence for GPU-side synchronization between
    // transient command buffer submissions and the main frame submission.
    // Each transient submission signals this fence with an incremented value,
    // and the main frame GPU-waits on it before executing its command buffer.
    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_transientSyncFence));
    if (FAILED(hr))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create transient sync fence", hr);
    }

    for (auto& value : m_transientSyncValues)
    {
        value = 0;
    }

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_commandBuffers[frameIndex] = MakeHandle<DX12CommandBuffer>(D3D12_COMMAND_LIST_TYPE_DIRECT);
        CheckResultOrReturn(m_commandBuffers[frameIndex]->Create());

#ifdef HYP_DEBUG_MODE
        wchar_t nameBuf[64];
        swprintf(nameBuf, std::size(nameBuf), L"Main CommandBuffer [frame=%u]", frameIndex);
        m_commandBuffers[frameIndex]->SetDebugName(nameBuf);
#endif
    }

    descriptorHeapManager->Initialize();

    CheckResultOrReturn(RenderInterface::Initialize());

    // In Direct3D, 256 is the minimum constant buffer alignment
    cbufferAllocator->Initialize(256);

    return {};
}

void DX12RenderInterface::Shutdown()
{
    HYP_LOG(RenderingBackend, Info, "Destroying DX12 render backend...");

    for (DX12AsyncCompute* ac : m_asyncComputePool)
    {
        delete ac;
    }

    for (DX12AsyncCompute* ac : m_submittedAsyncComputes)
    {
        delete ac;
    }

    m_asyncComputePool.Clear();
    m_submittedAsyncComputes.Clear();

    for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            m_transientCommandBuffers[threadIndex][frameIndex].Clear();
            m_pendingTransientCommandBuffers[threadIndex][frameIndex].Clear();
        }
    }

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_transientCommandBufferFences[frameIndex].Clear();
    }

    m_recycledTransientCommandBufferFences.Clear();

    for (DX12CommandBufferRef& commandBuffer : m_commandBuffers)
    {
        commandBuffer.Reset();
    }

    for (DX12FrameRef& frame : m_frames)
    {
        frame.Reset();
    }

    // cleanup frame fence
    if (m_frameFenceEvent != nullptr)
    {
        CloseHandle(m_frameFenceEvent);
        m_frameFenceEvent = nullptr;
    }
    m_frameFence.Reset();
    m_transientSyncFence.Reset();

    m_queueData = {};

    m_gpuTimerBackend->Shutdown();
    PoolDelete(*g_renderPool, m_gpuTimerBackend);

    RenderInterface::Shutdown();

    DeletionQueue::GetInstance().Shutdown();

    descriptorHeapManager->Shutdown();
    PoolDelete(*g_renderPool, descriptorHeapManager);

    m_allocator->Release();
    m_allocator = nullptr;

    m_dredSettings.Reset();

    m_device.Reset();
    m_hardwareAdapter.Reset();

    dxgiFactory.Reset();
}

const IRenderConfig& DX12RenderInterface::GetRenderConfig() const
{
    return *m_renderConfig;
}

bool DX12RenderInterface::CheckDeviceRemoved() const
{
    return CheckDeviceRemovedReason(m_device.Get()) != nullptr;
}

DX12Frame* DX12RenderInterface::GetCurrentFrame() const
{
    if (m_frames.Empty())
        return nullptr;

    return m_frames[GetFrameCounter() % NumFramesInFlight].Get();
}
HYP_DISABLE_OPTIMIZATION;

void DX12RenderInterface::PrepareFrame(DX12Frame* frame)
{
    const uint32 frameCounter = GetFrameCounter();
    const uint32 frameIndex = frameCounter % NumFramesInFlight;

    if (m_submissionFrames[frameIndex] >= 0)
    {
        const uint64 waitForValue = uint64(m_submissionFrames[frameIndex]) + 1;
        const uint64 currValue = m_frameFence->GetCompletedValue();

        if (currValue < waitForValue)
        {
            HRESULT hr = m_frameFence->SetEventOnCompletion(waitForValue, m_frameFenceEvent);
            if (FAILED(hr))
            {
                HYP_LOG(RenderingBackend, Error, "Failed to set fence completion event! Error: {}", hr);

                if (crashHandler)
                {
                    crashHandler->Dump();
                }

                const char* deviceRemovedReason = CheckDeviceRemovedReason(m_device.Get());
                if (deviceRemovedReason)
                {
                    HYP_LOG(RenderingBackend, Fatal, "Device removed: {}", deviceRemovedReason);
                }
            }

            DWORD waitResult = WaitForSingleObject(m_frameFenceEvent, INFINITE);
            if (waitResult != WAIT_OBJECT_0)
            {
                HYP_LOG(RenderingBackend, Error, "Failed to wait for fence! Result: {}", waitResult);

                if (crashHandler)
                {
                    crashHandler->Dump();
                }

                const char* deviceRemovedReason = CheckDeviceRemovedReason(m_device.Get());
                if (deviceRemovedReason)
                {
                    HYP_LOG(RenderingBackend, Fatal, "Device removed: {}", deviceRemovedReason);
                }
            }
        }

        // HYP_LOG_TEMP("Waited on {} on frame {}", waitForValue, frameIndex);
    }

    // call frame callbacks after fence is waited on
    if (frame->OnFrameEnd.AnyBound())
    {
        frame->OnFrameEnd(frame);
        frame->OnFrameEnd.RemoveAllDetached();
    }

    for (auto it = m_submittedAsyncComputes.Begin(); it != m_submittedAsyncComputes.End();)
    {
        DX12AsyncCompute* elem = *it;

        if (elem->CheckStatus())
        {
            elem->OnCompleted();
            elem->OnCompleted.RemoveAllDetached();

            AssertDebug(!elem->OnCompleted.AnyBound());

            // @NOTE Don't need to lock mutex since we'll only be using CreateAsyncCompute() from main render thread and render task / workers.
            // And workers wouldn't be kicked off at this point in the frame.
            m_asyncComputePool.PushBack(elem);

            it = m_submittedAsyncComputes.Erase(it);

            continue;
        }

        ++it;
    }

    // trim async compute pool if > 10 items
    if (m_asyncComputePool.Size() > 10)
    {
        static constexpr uint32 MaxFramesBeforeDiscard = 100;

        const uint32 currFrameIndex = GetFrameCounter();

        for (auto it = m_asyncComputePool.Begin(); it != m_asyncComputePool.End();)
        {
            DX12AsyncCompute* elem = *it;

            if (currFrameIndex - elem->lastFrame >= MaxFramesBeforeDiscard)
            {
                delete elem;

                it = m_asyncComputePool.Erase(it);

                continue;
            }

            ++it;
        }
    }

    // Wait on all fences for the frame that is about to be reused (the frame that was submitted NumFramesInFlight frames ago).
    auto& fences = m_transientCommandBufferFences[frameIndex];
    for (auto it = fences.Begin(); it != fences.End();)
    {
        DX12Fence& fence = *it;
        //HYP_LOG_TEMP("Waiting on transient command buffer {}, wait for fence value {} on frame {}", fence.GetDebugName(), fence.GetValue(), frameIndex);

        fence.Wait(true);

        m_recycledTransientCommandBufferFences.PushBack(std::move(fence));

        it = fences.Erase(it);
    }

    for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
    {
        LinkedList<DX12CommandBuffer, RenderAllocator>& freeList = m_transientCommandBuffers[threadIndex][frameIndex];
        LinkedList<DX12CommandBuffer, RenderAllocator>& pendingList = m_pendingTransientCommandBuffers[threadIndex][frameIndex];

        for (auto it = pendingList.Begin(); it != pendingList.End();)
        {
            DX12CommandBuffer& commandBuffer = *it;
            Assert(!commandBuffer.IsRecording());

            freeList.EmplaceBack(std::move(*it));

            it = pendingList.Erase(it);
        }
    }

    frame->OnFrameStart();

    // Reset transient sync value for this frame index — the old frame's
    // transient submissions have been CPU-waited on above, so the sync value
    // from NumFramesInFlight ago is no longer needed.
    m_transientSyncValues[frameIndex] = 0;
}

DX12SwapchainRef DX12RenderInterface::CreateSwapchain(ApplicationWindow* window, const Vec2u& extent)
{
    Assert(window != nullptr);

    DX12SwapchainRef swapchain = MakeHandle<DX12Swapchain>(window->GetHWND(), extent);
    RendererResult result = swapchain->Create();

    if (!result)
    {
        HYP_FAIL("Failed to create DX12 swapchain: {}", result.GetError().GetMessage());
    }

    return swapchain;
}

void DX12RenderInterface::PrepareSwapchain(DX12Swapchain* swapchain)
{
    swapchain->PrepareForFrame(GetCurrentFrame());
}

void DX12RenderInterface::PresentToSwapchain(DX12Swapchain* swapchain)
{
    DX12CommandBuffer* commandBuffer = GetCurrentCommandBuffer();
    AssertDebug(commandBuffer != nullptr);
    AssertDebug(!commandBuffer->IsRecording());

    DX12Frame* frame = GetCurrentFrame();
    Assert(frame != nullptr);

    const uint32 frameCounter = GetFrameCounter();
    const uint32 frameIndex = frameCounter % NumFramesInFlight;

    RI.InsertTransientSyncBarrier();

    const DX12QueueData* queueData = RI.GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    Assert(queueData != nullptr);

    const uint64 signalValue = uint64(frameCounter) + 1;
    commandBuffer->Submit(queueData->commandQueue.Get(), m_frameFence.Get(), signalValue);

    // HYP_LOG_TEMP("Signalling {} on frame {}", signalValue, frameIndex);

    m_submissionFrames[frameIndex] = int64(frameCounter);

    if (swapchain != nullptr)
    {
        swapchain->PresentFrame(frame);
    }
}

DX12CommandBuffer& DX12RenderInterface::GetTransientCommandBuffer()
{
    // usable from main render thread or renderer worker threads.
    const uint32 frameCounter = GetFrameCounter();
    const uint32 frameIndex = frameCounter % NumFramesInFlight;

    const uint32 renderThreadIndex = CurrentRenderThreadIndex();

    LinkedList<DX12CommandBuffer, RenderAllocator>& freeList = m_transientCommandBuffers[renderThreadIndex][frameIndex];
    LinkedList<DX12CommandBuffer, RenderAllocator>& pendingList = m_pendingTransientCommandBuffers[renderThreadIndex][frameIndex];

    DX12CommandBuffer* pCommandBuffer = nullptr;

    if (freeList.Any())
    {
        pCommandBuffer = &pendingList.PushBack(freeList.PopBack());
    }
    else
    {
        pCommandBuffer = &pendingList.EmplaceBack(D3D12_COMMAND_LIST_TYPE_DIRECT);
        CheckResult(pCommandBuffer->Create());
    }

    pCommandBuffer->Begin();
    BindDescriptorHeaps(*pCommandBuffer);

#ifdef HYP_DEBUG_MODE
    {
        wchar_t nameBuf[128];
        swprintf(nameBuf, std::size(nameBuf), L"Transient CommandBuffer [thread=%u][frame=%u]",
            renderThreadIndex, frameIndex);
        pCommandBuffer->SetDebugName(nameBuf);
    }
#endif

    return *pCommandBuffer;
}

void DX12RenderInterface::SubmitTransientCommandBuffer(DX12CommandBuffer& commandBuffer)
{
    // Transient command buffers are submitted to the DIRECT queue alongside
    // the main command buffer. A dedicated sync fence (m_transientSyncFence)
    // is signaled after each transient submission, and the main frame
    // GPU-waits on it via InsertTransientSyncBarrier() before executing.
    // This ensures the main rendering sees all transient work.

    const uint32 frameCounter = GetFrameCounter();
    const uint32 frameIndex = frameCounter % NumFramesInFlight;

    if (commandBuffer.IsRecording())
    {
        commandBuffer.End();
    }

    const DX12QueueData* queueData = GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    AssertDebug(queueData != nullptr);

    DX12Fence* pTransientFence = nullptr;

    {
        Mutex::Guard guard(m_transientCommandBuffersMutex);

        DX12Fence& fence = m_transientCommandBufferFences[frameIndex].EmplaceBack();

        if (m_recycledTransientCommandBufferFences.Any())
        {
            fence = m_recycledTransientCommandBufferFences.PopFront();
        }
        else
        {
            fence.Create();

#ifdef HYP_DEBUG_MODE
            wchar_t fenceNameBuf[64];
            swprintf(fenceNameBuf, std::size(fenceNameBuf), L"Transient Fence [frame=%u]", frameIndex);
            fence.SetDebugName(fenceNameBuf);
#endif
        }

        pTransientFence = &fence;
    }

    ID3D12CommandList* commandLists[] = { commandBuffer.GetCommandList() };
    queueData->commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);

    pTransientFence->Increment();

    // HYP_LOG_TEMP("Submitting transient command buffer {} with value {} on frame {}", pTransientFence->GetDebugName(), pTransientFence->GetValue(), frameIndex);

    HRESULT hr = queueData->commandQueue->Signal(pTransientFence->GetD3D12Fence(), pTransientFence->GetValue());
    if (FAILED(hr))
    {
        HYP_LOG(RenderingBackend, Error, "Failed to signal fence after executing command lists! Error: {}", hr);

        if (crashHandler)
        {
            crashHandler->Dump();
        }

        const char* deviceRemovedReason = CheckDeviceRemovedReason(m_device.Get());
        if (deviceRemovedReason)
        {
            HYP_LOG(RenderingBackend, Fatal, "Device removed: {}", deviceRemovedReason);
        }
    }

    // Signal the transient sync fence so that the main frame submission can wait on this value and guarantee ordering.
    {
        uint64& syncValue = m_transientSyncValues[frameIndex];
        ++syncValue;

        hr = queueData->commandQueue->Signal(m_transientSyncFence.Get(), syncValue);
        if (FAILED(hr))
        {
            HYP_LOG(RenderingBackend, Error, "Failed to signal transient sync fence! Error: {}", hr);

            if (crashHandler)
            {
                crashHandler->Dump();
            }

            const char* deviceRemovedReason = CheckDeviceRemovedReason(m_device.Get());
            if (deviceRemovedReason)
            {
                HYP_LOG(RenderingBackend, Fatal, "Device removed: {}", deviceRemovedReason);
            }
        }
    }
}

void DX12RenderInterface::BindDescriptorHeaps(DX12CommandBuffer& commandBuffer)
{
    ID3D12DescriptorHeap* viewHeap = descriptorHeapManager->GetDescriptorHeap(DX12DescriptorHeapType::CBV_SRV_UAV);
    Assert(viewHeap != nullptr);

    ID3D12DescriptorHeap* samplerHeap = descriptorHeapManager->GetDescriptorHeap(DX12DescriptorHeapType::SAMPLER);
    Assert(samplerHeap != nullptr);

    // Only bind descriptor heaps if they have changed to avoid unnecessary API calls
    if (commandBuffer.GetBoundViewHeap() != viewHeap || commandBuffer.GetBoundSamplerHeap() != samplerHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { viewHeap, samplerHeap };
        commandBuffer.GetCommandList()->SetDescriptorHeaps(UINT(std::size(heaps)), heaps);

        commandBuffer.SetBoundDescriptorHeaps(viewHeap, samplerHeap);
    }
}

DX12DescriptorSetRef DX12RenderInterface::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    return MakeHandle<DX12DescriptorSet>(layout);
}

DX12DescriptorTableRef DX12RenderInterface::MakeDescriptorTable(const ShaderInputGroup* decl)
{
    return MakeHandle<DX12DescriptorTable>(decl);
}

DX12GraphicsPipelineRef DX12RenderInterface::MakeGraphicsPipeline(
    const DX12ShaderInstanceRef& shaderInstance,
    const FramebufferDesc& framebufferDesc,
    const RenderableAttributeSet& attributes)
{
    DX12GraphicsPipelineRef graphicsPipeline = MakeHandle<DX12GraphicsPipeline>();

    if (shaderInstance.IsValid())
    {
        graphicsPipeline->SetShader(shaderInstance);

#ifdef HYP_DEBUG_MODE
        graphicsPipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shaderInstance->GetDebugName().IsValid() ? *shaderInstance->GetDebugName() : "<unnamed shader>"));
#endif
    }

    graphicsPipeline->SetFramebufferDesc(framebufferDesc);

    graphicsPipeline->SetInputLayout(attributes.GetMeshAttributes().inputLayout);
    graphicsPipeline->SetTopology(attributes.GetMeshAttributes().topology);

    graphicsPipeline->SetCullMode(attributes.GetMaterialAttributes().cullFaces);
    graphicsPipeline->SetFillMode(attributes.GetMaterialAttributes().fillMode);
    graphicsPipeline->SetBlendFunction(attributes.GetMaterialAttributes().blendFunction);
    graphicsPipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    graphicsPipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    graphicsPipeline->SetDepthCompareOp(attributes.GetMaterialAttributes().depthCompareOp);
    graphicsPipeline->SetDepthClamp(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP));

    if (attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
    {
        graphicsPipeline->SetDepthBias(attributes.GetMaterialAttributes().depthBias);
        graphicsPipeline->SetDepthBiasSlope(attributes.GetMaterialAttributes().depthBiasSlope);
    }

    if (attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)
    {
        graphicsPipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);
    }

    // for materials that write a stencil reference value
    if (attributes.GetMaterialAttributes().stencilReference != 0)
    {
        graphicsPipeline->SetStencilWrite(true);
    }

    // sanity check: newly created pipeline must match or caching will fail.
    AssertDebug(graphicsPipeline->MatchesSignature(attributes, framebufferDesc));

    return graphicsPipeline;
}

DX12ComputePipelineRef DX12RenderInterface::MakeComputePipeline(const DX12ShaderInstanceRef& shaderInstance)
{
    return MakeHandle<DX12ComputePipeline>(shaderInstance);
}

DX12RayTracingPipelineRef DX12RenderInterface::MakeRayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance)
{
    return MakeHandle<DX12RayTracingPipeline>(shaderInstance);
}

DX12GpuBufferRef DX12RenderInterface::MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment)
{
    return MakeHandle<DX12GpuBuffer>(bufferType, size, alignment);
}

DX12GpuImageRef DX12RenderInterface::MakeImage(const TextureDesc& textureDesc)
{
    return MakeHandle<DX12GpuImage>(textureDesc);
}

DX12GpuImageViewRef DX12RenderInterface::MakeImageView(const DX12GpuImageRef& image)
{
    return MakeHandle<DX12GpuImageView>(image);
}

DX12GpuImageViewRef DX12RenderInterface::MakeImageView(
    const DX12GpuImageRef& image,
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

    return MakeHandle<DX12GpuImageView>(image, subResource, viewType);
}

DX12SamplerRef DX12RenderInterface::MakeSampler(const SamplerDesc& samplerDesc)
{
    return MakeHandle<DX12Sampler>(samplerDesc);
}

DX12FramebufferRef DX12RenderInterface::MakeFramebuffer(const FramebufferDesc& framebufferDesc)
{
    return MakeHandle<DX12Framebuffer>(framebufferDesc);
}

DX12FrameRef DX12RenderInterface::MakeFrame(uint32 frameIndex)
{
    return MakeHandle<DX12Frame>(frameIndex);
}

DX12ShaderInstanceRef DX12RenderInterface::MakeShader(const Shader* shader)
{
    return MakeHandle<DX12ShaderInstance>(shader);
}

DX12GpuBlasRef DX12RenderInterface::MakeGpuBlas(
    const DX12GpuBufferRef& packedVerticesBuffer,
    const DX12GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<MaterialInstance>& material,
    const Mat4f& transform)
{
    return MakeHandle<DX12GpuBlas>(
        packedVerticesBuffer,
        packedIndicesBuffer,
        numVertices,
        numIndices,
        material,
        transform);
}

DX12GpuTlasRef DX12RenderInterface::MakeTLAS()
{
    return MakeHandle<DX12GpuTlas>();
}

void DX12RenderInterface::PopulateIndirectDrawCommandsBuffer(
    const DX12GpuBuffer* vertexBuffer,
    const DX12GpuBuffer* indexBuffer,
    uint32 instanceOffset,
    Array<D3D12_DRAW_INDEXED_ARGUMENTS, DX12Allocator>& outBuffer)
{
    const size_t requiredSize = (size_t(instanceOffset) + 1);

    if (outBuffer.Size() < requiredSize)
    {
        outBuffer.ResizeUninitialized(requiredSize);
    }

    uint32 numIndices = 0;

    if (indexBuffer != nullptr)
    {
        numIndices = uint32(indexBuffer->Size() / sizeof(uint32));
    }

    D3D12_DRAW_INDEXED_ARGUMENTS& command = outBuffer[instanceOffset];
    command = D3D12_DRAW_INDEXED_ARGUMENTS {};
    command.IndexCountPerInstance = numIndices;
    command.InstanceCount = 1;
    command.StartIndexLocation = 0;
    command.BaseVertexLocation = 0;
    command.StartInstanceLocation = 0;
}

bool DX12RenderInterface::IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
{
    DX12ViewType viewType = DX12ViewType::SRV_UAV;

    if (supportType == ImageSupport::Attachment)
    {
        viewType = DX12ViewType::RTV_DSV;
    }

    DXGI_FORMAT dxgiFormat = ToDXGIFormat(format, viewType);

    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport {};
    formatSupport.Format = dxgiFormat;

    HRESULT hr = m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    if (!SUCCEEDED(hr))
    {
        return false;
    }

    D3D12_FORMAT_SUPPORT1 support1 = formatSupport.Support1;
    D3D12_FORMAT_SUPPORT2 support2 = formatSupport.Support2;

    switch (supportType)
    {
    case ImageSupport::ShaderResource:
        return (support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) != 0 ||
               (support1 & D3D12_FORMAT_SUPPORT1_TEXTURE3D) != 0 ||
               (support1 & D3D12_FORMAT_SUPPORT1_TEXTURECUBE) != 0;
    case ImageSupport::Attachment:
        return (support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0 ||
               (support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) != 0;
    case ImageSupport::UnorderedAccess:
        return (support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) != 0;
    default:
        return false;
    }
}

TextureFormat DX12RenderInterface::FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
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

UniquePtr<SingleTimeCommands> DX12RenderInterface::GetSingleTimeCommands()
{
    return MakeUnique<DX12SingleTimeCommands>();
}

HYP_NODISCARD DX12AsyncCompute* DX12RenderInterface::CreateAsyncCompute()
{
    {
        Mutex::Guard guard(m_asyncComputesMutex);

        if (m_asyncComputePool.Any())
        {
            return m_asyncComputePool.PopBack();
        }
    }

    DX12AsyncCompute* newAsyncCompute = new DX12AsyncCompute();
    newAsyncCompute->Create();

    return newAsyncCompute;
}

void DX12RenderInterface::SubmitAsyncCompute(DX12AsyncCompute* asyncCompute)
{
    Assert(asyncCompute != nullptr);

    Mutex::Guard guard(m_asyncComputesMutex);
    Assert(!m_submittedAsyncComputes.Contains(asyncCompute));

    asyncCompute->Submit();

    m_submittedAsyncComputes.PushBack(asyncCompute);
}

void DX12RenderInterface::RecordStartTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    // @TODO
}

void DX12RenderInterface::RecordStopTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    // @TODO
}

void DX12RenderInterface::ResolveGpuFrameResults(uint32 completedFrameIndex)
{
    // @todo:
}

void DX12RenderInterface::ReleaseTransientMemory()
{
    GetCurrentFrame()->ResetTransientStates();
}

void DX12RenderInterface::BeginFrame(AtomicFlag* pCancelFlag)
{
    RenderInterface::BeginFrame(pCancelFlag);

    // Rebind descriptor heaps after command buffer reset in BeginFrame()
    BindDescriptorHeaps(*GetCurrentCommandBuffer());
}

void DX12RenderInterface::InsertTransientSyncBarrier()
{
    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;
    const uint64 syncValue = m_transientSyncValues[frameIndex];

    if (syncValue > 0 && m_transientSyncFence != nullptr)
    {
        const DX12QueueData* queueData = GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
        Assert(queueData != nullptr);

        queueData->commandQueue->Wait(m_transientSyncFence.Get(), syncValue);
    }
}

#pragma endregion DX12RenderInterface

void DX12RenderInterface::InitDeviceDetails(DeviceDetails& deviceDetails)
{
    DXGI_ADAPTER_DESC1 adapterDesc;
    m_hardwareAdapter->GetDesc1(&adapterDesc);

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 {};
    m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 {};
    m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

    bool isSoftware = (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;

    GpuInfo info;
    info.gpuType = isSoftware ? GpuType::Integrated : GpuType::Dedicated;
    info.vendorId = adapterDesc.VendorId;
    info.deviceId = adapterDesc.DeviceId;
    info.gpuModel = String(adapterDesc.Description);
    info.isDiscrete = !isSoftware;
    info.supportsRayTracing = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

    deviceDetails.Set(info);
}

} // namespace Hyperion
