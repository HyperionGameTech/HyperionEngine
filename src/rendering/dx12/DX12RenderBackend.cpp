/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12AccelerationStructure.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>

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
    : m_renderConfig(MakePimpl<DX12RenderConfig>()),
      m_currentFrameIndex(0),
      m_allocator(nullptr)
{
}

DX12RenderBackend::~DX12RenderBackend()
{
}

RendererResult DX12RenderBackend::Initialize()
{
    HYP_LOG(RenderingBackend, Info, "Initializing DX12 render backend...");

    uint32 createFactoryFlags = 0;

#ifdef HYP_DEBUG_MODE
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT res = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create DXGI Factory", res);

    ComPtr<IDXGIFactory6> factory6;

    if (SUCCEEDED(m_dxgiFactory.As(&factory6)))
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

            if (SUCCEEDED(D3D12CreateDevice(m_hardwareAdapter.Get(),  D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
                break;
        }
    } 
    else
    {
        for (UINT i = 0; SUCCEEDED(m_dxgiFactory->EnumAdapters1(i, &m_hardwareAdapter)); ++i) 
        {
            DXGI_ADAPTER_DESC1 desc;
            m_hardwareAdapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
                break;
        }
    }

    // create device
    res = D3D12CreateDevice(m_hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D device!", res);

    // create queues
    D3D12_COMMAND_QUEUE_DESC directDesc {};
    directDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    directDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    m_device->CreateCommandQueue(&directDesc, IID_PPV_ARGS(&m_directQueue));

    D3D12_COMMAND_QUEUE_DESC computeDesc {};
    computeDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    m_device->CreateCommandQueue(&computeDesc, IID_PPV_ARGS(&m_computeQueue));

    D3D12_COMMAND_QUEUE_DESC copyDesc {};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    m_device->CreateCommandQueue(&copyDesc, IID_PPV_ARGS(&m_copyQueue));

    D3D12MA::ALLOCATOR_DESC allocatorDesc {};
    allocatorDesc.pDevice = m_device.Get();
    allocatorDesc.pAdapter = m_hardwareAdapter.Get();

    res = D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D12MemoryAllocator instance!", res);

    return {};
}

RendererResult DX12RenderBackend::Destroy()
{
    HYP_LOG(RenderingBackend, Info, "Destroying DX12 render backend...");

    m_directQueue.Reset();
    m_computeQueue.Reset();
    m_copyQueue.Reset();

    m_allocator->Release();
    m_allocator = nullptr;

    m_device.Reset();
    m_hardwareAdapter.Reset();
    
    m_dxgiFactory.Reset();

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
    // @TODO: Implement frame preparation for DX12
    return GetCurrentFrame();
}

DX12SwapchainRef DX12RenderBackend::CreateSwapchain(ApplicationWindow* window)
{
    Assert(window != nullptr);
    
    // @TODO
    return DX12SwapchainRef();
}

void DX12RenderBackend::PrepareSwapchain(DX12Swapchain* swapchain)
{
    // @TODO: Implement swapchain preparation for DX12
}

void DX12RenderBackend::SubmitCommandBuffers(DX12Swapchain* swapchain)
{
    // @TODO: Implement command buffer submission for DX12
}

void DX12RenderBackend::PresentToSwapchain(DX12Swapchain* swapchain)
{
    // @TODO: Implement present for DX12
}

DX12CommandBuffer* DX12RenderBackend::GetCurrentCommandBuffer() const
{
    return m_commandBuffers[m_currentFrameIndex].Get();
}

DX12DescriptorSetRef DX12RenderBackend::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    // @TODO: Implement descriptor set creation for DX12
    return DescriptorSetRef();
}

DX12DescriptorTableRef DX12RenderBackend::MakeDescriptorTable(const DescriptorTableDeclaration* decl)
{
    // @TODO: Implement descriptor table creation for DX12
    return DescriptorTableRef();
}

DX12GraphicsPipelineRef DX12RenderBackend::MakeGraphicsPipeline(
    const DX12ShaderRef& shader,
    const RenderTargetDesc& renderTargetDesc,
    const RenderableAttributeSet& attributes)
{
    // @TODO: Implement graphics pipeline creation for DX12
    return GraphicsPipelineRef();
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

DX12ShaderRef DX12RenderBackend::MakeShader(const RC<CompiledShader>& compiledShader)
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
    // @TODO: Return proper default formats for DX12
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
