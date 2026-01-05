/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Frame.hpp>

#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderConfig.hpp>

#include <core/logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

#pragma region DX12RenderConfig

class DX12RenderConfig final : public IRenderConfig
{
public:
    DX12RenderConfig()
    {
        uniqueDrawCallPerMaterial = true;
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

        HYPERION_RETURN_OK;
    }
};

#pragma endregion DX12SingleTimeCommands

#pragma region DX12RenderBackend

DX12RenderBackend::DX12RenderBackend()
    : m_renderConfig(MakePimpl<DX12RenderConfig>()),
      m_currentFrameIndex(0)
{
}

DX12RenderBackend::~DX12RenderBackend()
{
}

RendererResult DX12RenderBackend::Initialize()
{
    HYP_LOG(RenderingBackend, Info, "Initializing DX12 render backend...");

    // @TODO: Initialize DX12 device, command queues, etc.

    HYPERION_RETURN_OK;
}

RendererResult DX12RenderBackend::Destroy()
{
    HYP_LOG(RenderingBackend, Info, "Destroying DX12 render backend...");

    // @TODO: Cleanup DX12 resources

    HYPERION_RETURN_OK;
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
    Span<const DX12FramebufferRef> framebuffers,
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
    // @TODO: Implement GPU buffer creation for DX12
    return GpuBufferRef();
}

DX12GpuImageRef DX12RenderBackend::MakeImage(const TextureDesc& textureDesc)
{
    // @TODO: Implement image creation for DX12
    return GpuImageRef();
}

DX12GpuImageViewRef DX12RenderBackend::MakeImageView(const DX12GpuImageRef& image)
{
    // @TODO: Implement image view creation for DX12
    return GpuImageViewRef();
}

DX12GpuImageViewRef DX12RenderBackend::MakeImageView(const DX12GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers)
{
    // @TODO: Implement image view creation for DX12
    return GpuImageViewRef();
}

DX12SamplerRef DX12RenderBackend::MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode)
{
    // @TODO: Implement sampler creation for DX12
    return SamplerRef();
}

DX12FramebufferRef DX12RenderBackend::MakeFramebuffer(Vec2u extent, uint32 numViews)
{
    // @TODO: Implement framebuffer creation for DX12
    return FramebufferRef();
}

DX12FramebufferRef DX12RenderBackend::MakeFramebuffer(Vec2u extent, RenderTargetType renderTargetType, uint32 numViews)
{
    // @TODO: Implement framebuffer creation for DX12
    return FramebufferRef();
}

DX12FrameRef DX12RenderBackend::MakeFrame(uint32 frameIndex)
{
    // @TODO: Implement frame creation for DX12
    return FrameRef();
}

DX12ShaderRef DX12RenderBackend::MakeShader(const RC<CompiledShader>& compiledShader)
{
    // @TODO: Implement shader creation for DX12
    return ShaderRef();
}

DX12GpuBlasRef DX12RenderBackend::MakeGpuBlas(
    const DX12GpuBufferRef& packedVerticesBuffer,
    const DX12GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
{
    // @TODO: Implement BLAS creation for DX12
    return GpuBlasRef();
}

DX12GpuTlasRef DX12RenderBackend::MakeTLAS()
{
    // @TODO: Implement TLAS creation for DX12
    return GpuTlasRef();
}

const DX12GpuImageViewRef& DX12RenderBackend::GetTextureImageView(const Handle<Texture>& texture, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers)
{
    // @TODO: Implement texture image view retrieval for DX12
    static GpuImageViewRef nullImageView;
    return nullImageView;
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
