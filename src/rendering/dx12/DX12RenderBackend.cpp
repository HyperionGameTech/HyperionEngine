/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12Swapchain.hpp>
#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12GraphicsPipeline.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <system/AppContext.hpp>

#include <core/logging/Logger.hpp>

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
        raytracing = false;
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

#pragma region DX12RenderBackend

DX12RenderBackend::DX12RenderBackend()
    : descriptorHeapManager(PoolNew<DX12DescriptorHeapManager>(*g_renderPool)),
      m_renderConfig(MakePimpl<DX12RenderConfig>()),
      m_currentFrameIndex(0),
      m_allocator(nullptr)
{
}

DX12RenderBackend::~DX12RenderBackend()
{
    PoolDelete(*g_renderPool, descriptorHeapManager);
}

RendererResult DX12RenderBackend::Initialize()
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

        frame = CreateObject<DX12Frame>(frameIndex);
        CheckResultOrReturn(frame->Create());
    }

    // create main commandlist
    m_commandBuffer = CreateObject<DX12CommandBuffer>(D3D12_COMMAND_LIST_TYPE_DIRECT);
    CheckResultOrReturn(m_commandBuffer->Create());

    descriptorHeapManager->Initialize();

    return {};
}

RendererResult DX12RenderBackend::Destroy()
{
    HYP_LOG(RenderingBackend, Info, "Destroying DX12 render backend...");

    descriptorHeapManager->Shutdown();

    m_commandBuffer.Reset();

    for (DX12FrameRef& frame : m_frames)
    {
        frame.Reset();
    }

    m_queueData = {};

    m_allocator->Release();
    m_allocator = nullptr;

    m_device.Reset();
    m_hardwareAdapter.Reset();
    
    dxgiFactory.Reset();

    return {};
}

const IRenderConfig& DX12RenderBackend::GetRenderConfig() const
{
    return *m_renderConfig;
}

AsyncComputeBase* DX12RenderBackend::GetAsyncCompute() const
{
    // @TODO: Implement async compute for DX12
    return nullptr;
}

DX12Frame* DX12RenderBackend::GetCurrentFrame() const
{
    return m_frames[m_currentFrameIndex].Get();
}

DX12Frame* DX12RenderBackend::PrepareNextFrame()
{
    DX12Frame* frame = GetCurrentFrame();

    const uint32 frameIndex = frame->GetFrameIndex();

    ID3D12CommandAllocator* commandAllocator = m_queueData[D3D12_COMMAND_LIST_TYPE_DIRECT].commandAllocators[frameIndex].Get();
    Assert(SUCCEEDED(commandAllocator->Reset()));

    Assert(SUCCEEDED(m_commandBuffer->GetCommandList()->Reset(commandAllocator, nullptr)));

    return frame;
}

DX12SwapchainRef DX12RenderBackend::CreateSwapchain(ApplicationWindow* window)
{
    Assert(window != nullptr);
    
    return CreateObject<DX12Swapchain>(window->GetHWND(), Vec2u(window->GetSize()));
}

void DX12RenderBackend::PrepareSwapchain(DX12Swapchain* swapchain)
{
    swapchain->PrepareForFrame(GetCurrentFrame());
}

void DX12RenderBackend::SubmitCommandBuffers(DX12Swapchain* swapchain)
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

void DX12RenderBackend::PresentToSwapchain(DX12Swapchain* swapchain)
{
    swapchain->PresentFrame(GetCurrentFrame());
}

DX12CommandBuffer* DX12RenderBackend::GetCurrentCommandBuffer() const
{
    return m_commandBuffer.Get();
}

DX12DescriptorSetRef DX12RenderBackend::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    return CreateObject<DX12DescriptorSet>(layout);
}

DX12DescriptorTableRef DX12RenderBackend::MakeDescriptorTable(const DescriptorTableDeclaration* decl)
{
    return CreateObject<DX12DescriptorTable>(decl);
}

DX12GraphicsPipelineRef DX12RenderBackend::MakeGraphicsPipeline(
    const DX12ShaderRef& shader,
    const RenderTargetDesc& renderTargetDesc,
    const RenderableAttributeSet& attributes)
{
    DX12GraphicsPipelineRef graphicsPipeline = CreateObject<DX12GraphicsPipeline>();

    if (shader.IsValid())
    {
        graphicsPipeline->SetShader(shader);

#ifdef HYP_DEBUG_MODE
        graphicsPipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shader->GetDebugName().IsValid() ? *shader->GetDebugName() : "<unnamed shader>"));
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

DX12ComputePipelineRef DX12RenderBackend::MakeComputePipeline(
    const DX12ShaderRef& shader,
    const DX12DescriptorTableRef& descriptorTable)
{
    // @TODO: Implement compute pipeline creation for DX12
    return ComputePipelineRef();
}

DX12RaytracingPipelineRef DX12RenderBackend::MakeRaytracingPipeline(
    const DX12ShaderRef& shader,
    const DX12DescriptorTableRef& descriptorTable)
{
    // @TODO: Implement raytracing pipeline creation for DX12
    return RaytracingPipelineRef();
}

DX12GpuBufferRef DX12RenderBackend::MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment)
{
    return CreateObject<DX12GpuBuffer>(bufferType, size, alignment);
}

DX12GpuImageRef DX12RenderBackend::MakeImage(const TextureDesc& textureDesc)
{
    return CreateObject<DX12GpuImage>(textureDesc);
}

DX12GpuImageViewRef DX12RenderBackend::MakeImageView(const DX12GpuImageRef& image)
{
    return CreateObject<DX12GpuImageView>(image);
}

DX12GpuImageViewRef DX12RenderBackend::MakeImageView(const DX12GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers)
{
    return CreateObject<DX12GpuImageView>(image, mipIndex, numMips, layerIndex, numLayers);
}

DX12SamplerRef DX12RenderBackend::MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode)
{
    return CreateObject<DX12Sampler>(filterModeMin, filterModeMag, wrapMode);
}

DX12FramebufferRef DX12RenderBackend::MakeFramebuffer(const RenderTargetDesc& renderTargetDesc)
{
    return CreateObject<DX12Framebuffer>(renderTargetDesc);
}

DX12FrameRef DX12RenderBackend::MakeFrame(uint32 frameIndex)
{
    return CreateObject<DX12Frame>(frameIndex);
}

DX12ShaderRef DX12RenderBackend::MakeShader(const CompiledShader* compiledShader)
{
    return CreateObject<DX12Shader>(compiledShader);
}

DX12GpuBlasRef DX12RenderBackend::MakeGpuBlas(
    const DX12GpuBufferRef& packedVerticesBuffer,
    const DX12GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
{
    return CreateObject<DX12GpuBlas>(
        packedVerticesBuffer,
        packedIndicesBuffer,
        numVertices,
        numIndices,
        material,
        transform);
}

DX12GpuTlasRef DX12RenderBackend::MakeTLAS()
{
    return CreateObject<DX12GpuTlas>();
}

void DX12RenderBackend::PopulateIndirectDrawCommandsBuffer(const DX12GpuBufferRef& vertexBuffer, const DX12GpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer)
{
    // @TODO: Implement indirect draw command buffer population for DX12
}

TextureFormat DX12RenderBackend::GetDefaultFormat(DefaultImageFormat type) const
{
    switch (type)
    {
    case DefaultImageFormat::DIF_COLOR:
        return TextureFormat::TF_RGBA8;
    case DefaultImageFormat::DIF_DEPTH:
        return TextureFormat::TF_DEPTH_32F;
    default:
        return TextureFormat::TF_NONE;
    }
}

bool DX12RenderBackend::IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
{
    // @TODO: Implement format support checking for DX12
    return false;
}

TextureFormat DX12RenderBackend::FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
{
    // @TODO: Implement supported format finding for DX12
    if (possibleFormats.Size() == 0)
    {
        return TextureFormat::TF_NONE;
    }

    return possibleFormats[0];
}

QueryImageCapabilitiesResult DX12RenderBackend::QueryImageCapabilities(const TextureDesc& textureDesc) const
{
    // @TODO: Implement image capabilities query for DX12
    QueryImageCapabilitiesResult result;
    result.supports2d = true;
    result.supports3d = true;
    result.supportsCubemap = true;
    result.supportsArray = true;
    result.supportsMipmaps = true;
    result.supportsStorage = true;

    return result;
}

UniquePtr<SingleTimeCommands> DX12RenderBackend::GetSingleTimeCommands()
{
    return MakeUnique<DX12SingleTimeCommands>();
}

void DX12RenderBackend::ReleaseTransientMemory()
{
    // @TODO: Implement transient memory release for DX12
}

void DX12RenderBackend::NextFrame()
{
    m_currentFrameIndex = (m_currentFrameIndex + 1) % NumFramesInFlight;
}

#pragma endregion DX12RenderBackend

} // namespace Hyperion
