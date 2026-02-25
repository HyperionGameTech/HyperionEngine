/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12Swapchain.hpp>
#include <rendering/dx12/DX12AsyncCompute.hpp>
#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12ShaderInstance.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderableAttributes.hpp>

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
      m_currentFrameIndex(0),
      m_allocator(nullptr)
{
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

    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings), &m_dredSettings)))
    {
        m_dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        m_dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

#ifdef HYP_DEBUG_MODE
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController)))
        debugController->EnableDebugLayer();
#endif

    // create device
    res = D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D device!", res);
    
    m_queueData.Reserve(10);

    DX12QueueData& directQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT];
    directQueueData = {};

    DX12QueueData& computeQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_COMPUTE];
    computeQueueData = {};

    DX12QueueData& copyQueueData = m_queueData[D3D12_COMMAND_LIST_TYPE_COPY];
    copyQueueData = {};

    // create queues and command allocators
    D3D12_COMMAND_QUEUE_DESC directDesc {};
    directDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    directDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    m_device->CreateCommandQueue(&directDesc, __uuidof(ID3D12CommandQueue), &directQueueData.commandQueue);

    D3D12_COMMAND_QUEUE_DESC computeDesc {};
    computeDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    m_device->CreateCommandQueue(&computeDesc, __uuidof(ID3D12CommandQueue), &computeQueueData.commandQueue);

    D3D12_COMMAND_QUEUE_DESC copyDesc {};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    m_device->CreateCommandQueue(&copyDesc, __uuidof(ID3D12CommandQueue), &copyQueueData.commandQueue);
    
    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        for (auto& pair : m_queueData)
        {
            D3D12_COMMAND_LIST_TYPE commandListType = pair.first;
            DX12QueueData& queueData = pair.second;

            res = m_device->CreateCommandAllocator(commandListType, __uuidof(ID3D12CommandAllocator), &queueData.commandAllocators[frameIndex]);

            if (!SUCCEEDED(res))
                return HYP_MAKE_ERROR(RendererError, "Failed to create command allocator for queue {}!", res, commandListType);
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

    // create main commandlist
    m_commandBuffer = MakeHandle<DX12CommandBuffer>(D3D12_COMMAND_LIST_TYPE_DIRECT);
    CheckResultOrReturn(m_commandBuffer->Create());

    descriptorHeapManager->Initialize();
    
    CheckResultOrReturn(RenderInterface::Initialize());

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

    descriptorHeapManager->Shutdown();

    m_commandBuffer.Reset();

    for (DX12FrameRef& frame : m_frames)
    {
        frame.Reset();
    }

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
    return m_frames[m_currentFrameIndex].Get();
}

DX12Frame* DX12RenderInterface::PrepareNextFrame()
{
    for (auto it = m_submittedAsyncComputes.Begin(); it != m_submittedAsyncComputes.End();)
    {
        DX12AsyncCompute* elem = *it;

        if (elem->CheckStatus())
        {
            elem->OnCompleted();

            m_asyncComputePool.PushBack(elem);

            it = m_submittedAsyncComputes.Erase(it);

            continue;
        }

        ++it;
    }

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

    DX12Frame* frame = GetCurrentFrame();

    const uint32 frameIndex = frame->GetFrameIndex();

    ID3D12CommandAllocator* commandAllocator = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT].commandAllocators[frameIndex].Get();
    Assert(SUCCEEDED(commandAllocator->Reset()));

    Assert(SUCCEEDED(m_commandBuffer->GetCommandList()->Reset(commandAllocator, nullptr)));

    return frame;
}

DX12SwapchainRef DX12RenderInterface::CreateSwapchain(ApplicationWindow* window)
{
    Assert(window != nullptr);
    
    return MakeHandle<DX12Swapchain>(window->GetHWND(), Vec2u(window->GetSize()));
}

void DX12RenderInterface::PrepareSwapchain(DX12Swapchain* swapchain)
{
    swapchain->PrepareForFrame(GetCurrentFrame());
}

void DX12RenderInterface::SubmitCommandBuffers(DX12Swapchain* swapchain)
{
    DX12Frame* currentFrame = GetCurrentFrame();
    const uint32 frameIndex = currentFrame->GetFrameIndex();

    DX12QueueData& queueData = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT];

    ID3D12CommandAllocator* allocator = queueData.commandAllocators[frameIndex].Get();
    AssertDebug(allocator != nullptr);

    Assert(SUCCEEDED(m_commandBuffer->GetCommandList()->Close()));

    ID3D12CommandList* commandLists[] = { m_commandBuffer->GetCommandList() };

    queueData.commandQueue->ExecuteCommandLists(ArraySize(commandLists), commandLists);
}

void DX12RenderInterface::PresentToSwapchain(DX12Swapchain* swapchain)
{
    swapchain->PresentFrame(GetCurrentFrame());
}

DX12CommandBuffer* DX12RenderInterface::GetCurrentCommandBuffer() const
{
    return m_commandBuffer.Get();
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
    const RenderTargetDesc& renderTargetDesc,
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

    graphicsPipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertexAttributes);
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

    graphicsPipeline->SetRenderTargetDesc(renderTargetDesc);

    return graphicsPipeline;
}

DX12ComputePipelineRef DX12RenderInterface::MakeComputePipeline(const DX12ShaderInstanceRef& shaderInstance)
{
    // @TODO: Implement compute pipeline creation for DX12
    return ComputePipelineRef();
}

DX12RayTracingPipelineRef DX12RenderInterface::MakeRayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance)
{
    // @TODO: Implement rayTracing pipeline creation for DX12
    return RayTracingPipelineRef();
}

DX12GpuBufferRef DX12RenderInterface::MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment)
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

DX12SamplerRef DX12RenderInterface::MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode)
{
    return MakeHandle<DX12Sampler>(filterModeMin, filterModeMag, wrapMode);
}

DX12FramebufferRef DX12RenderInterface::MakeFramebuffer(const RenderTargetDesc& renderTargetDesc)
{
    return MakeHandle<DX12Framebuffer>(renderTargetDesc);
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
    const Handle<Material>& material,
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
    // @TODO: Implement indirect draw command buffer population for DX12
}

TextureFormat DX12RenderInterface::GetDefaultFormat(DefaultImageFormat type) const
{
    switch (type)
    {
    case DefaultImageFormat::DIF_COLOR:
        return TextureFormat::RGBA8;
    case DefaultImageFormat::DIF_DEPTH:
        return TextureFormat::D24_S8;
    default:
        return InvalidTextureFormat;
    }
}

bool DX12RenderInterface::IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
{
    // @TODO: Implement format support checking for DX12
    return false;
}

TextureFormat DX12RenderInterface::FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
{
    // @TODO: Implement supported format finding for DX12
    if (possibleFormats.Size() == 0)
    {
        return InvalidTextureFormat;
    }

    return possibleFormats[0];
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
    // @TODO: Implement transient memory release for DX12
}

void DX12RenderInterface::NextFrame()
{
    m_currentFrameIndex = (m_currentFrameIndex + 1) % NumFramesInFlight;
}

#pragma endregion DX12RenderInterface

} // namespace Hyperion
