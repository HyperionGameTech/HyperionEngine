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
#include <rendering/dx12/DX12Fence.hpp>
#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12ComputePipeline.hpp>
#include <rendering/dx12/DX12ShaderInstance.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/CBufferAllocator.hpp>

#include <Core/threading/AtomicFlag.hpp>

#include <engine/DeviceDetails.hpp>

#include <system/AppContext.hpp>

#include <Core/logging/Logger.hpp>

#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

#pragma region DX12RenderConfig

class DX12RenderConfig final : public IRenderConfig
{
public:
    DX12RenderConfig()
    {
        bindlessTextures = false;
        rayTracing = false;
        indirectRendering = false;
        parallelRendering = false;
        dynamicDescriptorIndexing = false;
    }

    void InitializeBindless(DX12RenderInterface* renderInterface)
    {
        ID3D12Device* device = renderInterface->GetDevice();

        // Resource Binding Tier 3 = unbounded descriptor tables (full bindless support)
        D3D12_FEATURE_DATA_D3D12_OPTIONS options {};
        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
        if (SUCCEEDED(hr))
        {
            bindlessTextures = (options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);
            dynamicDescriptorIndexing = (options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_2);
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
        // @TODO: Implement single time commands for DX12
        HYP_LOG(RenderingBackend, Warning, "DX12SingleTimeCommands::Execute() not implemented");

        return {};
    }
};

#pragma endregion DX12SingleTimeCommands

#pragma region DX12RenderInterface

DX12RenderInterface::DX12RenderInterface()
    : descriptorHeapManager(PoolNew<DX12DescriptorHeapManager>(*g_renderPool)),
      m_renderConfig(MakePimpl<DX12RenderConfig>()),
      m_allocator(nullptr),
      m_frameFenceEvent(nullptr)
{
    // Initialize fence values to 0 (no submissions yet)
    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        m_frameFenceValues[i] = 0;
        m_frameFenceSubmitted[i] = false;
    }
}

DX12RenderInterface::~DX12RenderInterface()
{
    PoolDelete(*g_renderPool, descriptorHeapManager);
}

RendererResult DX12RenderInterface::Initialize()
{
    HYP_LOG(RenderingBackend, Info, "Initializing DX12 render backend...");

    uint32 createFactoryFlags = 0;

#ifdef HYP_DEBUG_MODE
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT res = CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), &dxgiFactory);
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create DXGI Factory", res);

    ComPtr<IDXGIFactory6> factory6;

    if (SUCCEEDED(dxgiFactory.As(&factory6)))
    {
        for (UINT i = 0;
             SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                 i,
                 DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                 IID_PPV_ARGS(&m_hardwareAdapter)));
             ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            m_hardwareAdapter->GetDesc1(&desc);
        
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(m_hardwareAdapter.Get(),  D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
                break;
        }
    } 
    else
    {
        for (UINT i = 0; SUCCEEDED(dxgiFactory->EnumAdapters1(i, &m_hardwareAdapter)); ++i) 
        {
            DXGI_ADAPTER_DESC1 desc;
            m_hardwareAdapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
                break;
        }
    }

#ifdef HYP_DEBUG_MODE
#if 1
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings), &m_dredSettings)))
    {
        m_dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        m_dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController)))
    {
        debugController->EnableDebugLayer();
    }
#endif // 
#endif

    // create device
    res = D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D device!", res);

    // Initialize render config features based on device capabilities
    static_cast<DX12RenderConfig*>(m_renderConfig.Get())->InitializeBindless(this);

    static_assert(sizeof(decltype(m_queueData)) / sizeof(decltype(m_queueData[0])) > D3D12_COMMAND_LIST_TYPE_COPY,
        "m_queueData is too small; must have size increased.");

    Memory::Zero(&m_queueData, sizeof(m_queueData));

    // create queues and command allocators
    D3D12_COMMAND_QUEUE_DESC directDesc {};
    directDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    directDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    DX12QueueData& directQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT];
    m_device->CreateCommandQueue(&directDesc, __uuidof(ID3D12CommandQueue), &directQueueData.commandQueue);

    D3D12_COMMAND_QUEUE_DESC computeDesc {};
    computeDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    DX12QueueData& computeQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_COMPUTE];
    m_device->CreateCommandQueue(&computeDesc, __uuidof(ID3D12CommandQueue), &computeQueueData.commandQueue);

    D3D12_COMMAND_QUEUE_DESC copyDesc {};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

    DX12QueueData& copyQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_COPY];
    m_device->CreateCommandQueue(&copyDesc, __uuidof(ID3D12CommandQueue), &copyQueueData.commandQueue);
    
    // Create command allocators for [threadIndex][frameIndex]
    
    for (uint32 commandListTypeIndex = 0; commandListTypeIndex < uint32(m_queueData.Size()); commandListTypeIndex++)
    {
        D3D12_COMMAND_LIST_TYPE commandListType = static_cast<D3D12_COMMAND_LIST_TYPE>(commandListTypeIndex);
        DX12QueueData& queueData = m_queueData[commandListTypeIndex];
        
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
            {
                res = m_device->CreateCommandAllocator(commandListType, __uuidof(ID3D12CommandAllocator), &queueData.commandAllocators[threadIndex][frameIndex]);

                if (!SUCCEEDED(res))
                    return HYP_MAKE_ERROR(RendererError, "Failed to create command allocator for queue {}!", res, commandListType);
            }
        }
    }

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

    m_frameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_frameFenceEvent == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create frame fence event");
    }

    // Create command buffers for each frame in flight
    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        ID3D12CommandAllocator* allocator = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT].commandAllocators[0][frameIndex].Get();
        AssertDebug(allocator != nullptr);

        m_commandBuffers[frameIndex] = MakeHandle<DX12CommandBuffer>(D3D12_COMMAND_LIST_TYPE_DIRECT, allocator);
        CheckResultOrReturn(m_commandBuffers[frameIndex]->Create());
    }

    descriptorHeapManager->Initialize();
    
    // In Direct3D, 256 is the minimum constant buffer alignment
    cbufferAllocator->Initialize(256);

    return RenderInterface::Initialize();
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

    for (DX12CommandBuffer* commandBuffer : m_ownedTransientCommandBuffers)
    {
        delete commandBuffer;
    }

    m_ownedTransientCommandBuffers.Clear();

    for (DX12Fence* fence : m_ownedTransientCommandBufferFences)
    {
        delete fence;
    }

    m_ownedTransientCommandBufferFences.Clear();

    for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            m_transientCommandBufferFences[threadIndex][frameIndex].Clear();

            m_transientCommandBuffers[threadIndex][frameIndex].Clear();
            m_pendingTransientCommandBuffers[threadIndex][frameIndex].Clear();
        }
    }

    m_recycledTransientCommandBufferFences.Clear();

    descriptorHeapManager->Shutdown();

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

    m_queueData = {};

    RenderInterface::Shutdown();

    m_allocator->Release();
    m_allocator = nullptr;

    m_device.Reset();
    m_hardwareAdapter.Reset();
    
    dxgiFactory.Reset();
}

const IRenderConfig& DX12RenderInterface::GetRenderConfig() const
{
    return *m_renderConfig;
}

DX12Frame* DX12RenderInterface::GetCurrentFrame() const
{
    return m_frames[GetFrameCounter() % NumFramesInFlight].Get();
}

void DX12RenderInterface::PrepareFrame(DX12Frame* frame)
{
    const uint32 frameCounter = GetFrameCounter();
    const uint32 frameIndex = frameCounter % NumFramesInFlight;

    if (m_frameFenceSubmitted[frameIndex])
    {
        const uint64 currentFenceValue = m_frameFenceValues[frameIndex];

        if (m_frameFence->GetCompletedValue() < currentFenceValue)
        {
            HRESULT hr = m_frameFence->SetEventOnCompletion(currentFenceValue, m_frameFenceEvent);
            Assert(SUCCEEDED(hr));

            DWORD waitResult = WaitForSingleObject(m_frameFenceEvent, INFINITE);
            Assert(waitResult == WAIT_OBJECT_0);
        }
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

    for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
    {
        Array<DX12Fence*, RenderAllocator>& fences = m_transientCommandBufferFences[threadIndex][frameCounter % NumFramesInFlight];
        for (DX12Fence* fence : fences)
        {
            fence->Wait(true);

            fence->Reset();

            m_recycledTransientCommandBufferFences.PushBack(fence);
        }
        fences.Clear();

        // reset our transient command buffers
        Array<DX12CommandBuffer*, RenderAllocator>& freeList = m_transientCommandBuffers[threadIndex][frameCounter % NumFramesInFlight];
        Array<DX12CommandBuffer*, RenderAllocator>& pendingList = m_pendingTransientCommandBuffers[threadIndex][frameCounter % NumFramesInFlight];

        for (DX12CommandBuffer* commandBuffer : pendingList)
        {
            Assert(!commandBuffer->IsRecording());

            freeList.PushBack(commandBuffer);
        }

        pendingList.Clear();
    }

    // Reset the direct queue's command allocators for the current frame across all threads
    {
        DX12QueueData& queueData = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT];

        for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
        {
            Assert(SUCCEEDED(queueData.commandAllocators[threadIndex][frameIndex]->Reset()));
        }
    }

    frame->OnFrameStart();
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
    AssertDebug(commandBuffer->IsRecording());

    DX12Frame* frame = GetCurrentFrame();
    Assert(frame != nullptr);

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    frame->WriteCommandBuffer(commandBuffer);

    const uint64 fenceValue = ++m_frameFenceValues[frameIndex];
    m_frameFenceSubmitted[frameIndex] = true;

    const DX12QueueData* queueData = GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);
    AssertDebug(queueData != nullptr);

    HRESULT hr = queueData->commandQueue->Signal(m_frameFence.Get(), fenceValue);
    Assert(SUCCEEDED(hr));

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

    // Use the [frameIndex][threadIndex] allocator
    ID3D12CommandAllocator* allocator = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT].commandAllocators[renderThreadIndex][frameIndex].Get();

    Array<DX12CommandBuffer*, RenderAllocator>& freeList = m_transientCommandBuffers[renderThreadIndex][frameIndex];
    Array<DX12CommandBuffer*, RenderAllocator>& pendingList = m_pendingTransientCommandBuffers[renderThreadIndex][frameIndex];

    DX12CommandBuffer* commandBuffer = nullptr;

    if (freeList.Any())
    {
        commandBuffer = freeList.PopBack();
    }
    else
    {
        commandBuffer = new DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE_DIRECT, allocator);
        CheckResult(commandBuffer->Create());

        Mutex::Guard guard(m_transientCommandBuffersMutex);
        m_ownedTransientCommandBuffers.PushBack(commandBuffer);
    }

    pendingList.PushBack(commandBuffer);

    commandBuffer->Begin();
    BindDescriptorHeaps(*commandBuffer);

    return *commandBuffer;
}

void DX12RenderInterface::SubmitTransientCommandBuffer(DX12CommandBuffer& commandBuffer)
{
    const uint32 frameCounter = GetFrameCounter();
    const uint32 renderThreadIndex = CurrentRenderThreadIndex();

    if (commandBuffer.IsRecording())
    {
        commandBuffer.End();
    }

    DX12QueueData& queueData = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT];

    ID3D12CommandList* commandLists[] = { commandBuffer.GetCommandList() };
    queueData.commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);

    DX12Fence* fence = nullptr;

    {
        Mutex::Guard guard(m_transientCommandBuffersMutex);

        if (m_recycledTransientCommandBufferFences.Any())
        {
            fence = m_recycledTransientCommandBufferFences.PopBack();
        }
    }

    if (fence == nullptr)
    {
        fence = new DX12Fence();
        CheckResult(fence->Create());

        Mutex::Guard guard(m_transientCommandBuffersMutex);
        m_ownedTransientCommandBufferFences.PushBack(fence);
    }

    CheckResult(fence->Reset());

    HRESULT hr = queueData.commandQueue->Signal(fence->GetD3D12Fence(), fence->GetValue());
    Assert(SUCCEEDED(hr));

    m_transientCommandBufferFences[renderThreadIndex][frameCounter % NumFramesInFlight].PushBack(fence);
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

    graphicsPipeline->SetInputLayout(attributes.GetMeshAttributes().inputLayout);
    graphicsPipeline->SetTopology(attributes.GetMeshAttributes().topology);

    graphicsPipeline->SetCullMode(attributes.GetMaterialAttributes().cullFaces);
    graphicsPipeline->SetFillMode(attributes.GetMaterialAttributes().fillMode);
    graphicsPipeline->SetBlendFunction(attributes.GetMaterialAttributes().blendFunction);
    graphicsPipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    graphicsPipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    
    if (attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)  
    {
        graphicsPipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);
    }

    // for materials that write a stencil reference value
    if (attributes.GetMaterialAttributes().stencilReference != 0)
    {
        graphicsPipeline->SetStencilWrite(true);
    }

    graphicsPipeline->SetFramebufferDesc(framebufferDesc);

    return graphicsPipeline;
}

DX12ComputePipelineRef DX12RenderInterface::MakeComputePipeline(const DX12ShaderInstanceRef& shaderInstance)
{
    return MakeHandle<DX12ComputePipeline>(shaderInstance);
}

DX12RayTracingPipelineRef DX12RenderInterface::MakeRayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance)
{
    // @TODO: Implement rayTracing pipeline creation for DX12
    return RayTracingPipelineRef();
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

DX12GpuImageViewRef DX12RenderInterface::MakeImageView(const DX12GpuImageRef& image, uint8 mipIndex, uint8 numMips, uint16 layerIndex, uint16 numLayers)
{
    ImageSubResource subResource {};
    subResource.baseMipLevel = mipIndex;
    subResource.baseArrayLayer = layerIndex;
    subResource.numLevels = numMips;
    subResource.numLayers = numLayers;

    return MakeHandle<DX12GpuImageView>(image, subResource);
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

void DX12RenderInterface::PopulateIndirectDrawCommandsBuffer(const DX12GpuBufferRef& vertexBuffer, const DX12GpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer)
{
    const size_t requiredSize = (size_t(instanceOffset) + 1) * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

    if (outByteBuffer.Size() < requiredSize)
    {
        outByteBuffer.SetSize(requiredSize);
    }

    uint32 numIndices = 0;

    if (indexBuffer.IsValid())
    {
        numIndices = uint32(indexBuffer->Size() / sizeof(uint32));
    }

    D3D12_DRAW_INDEXED_ARGUMENTS* commandPtr = reinterpret_cast<D3D12_DRAW_INDEXED_ARGUMENTS*>(outByteBuffer.Data()) + instanceOffset;
    *commandPtr = D3D12_DRAW_INDEXED_ARGUMENTS {};
    commandPtr->IndexCountPerInstance = numIndices;
    commandPtr->InstanceCount = 1;
    commandPtr->StartIndexLocation = 0;
    commandPtr->BaseVertexLocation = 0;
    commandPtr->StartInstanceLocation = 0;
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

DX12AsyncCompute* DX12RenderInterface::CreateAsyncCompute()
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
