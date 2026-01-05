/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderBackend.hpp>

#include <core/memory/Pimpl.hpp>

namespace Hyperion {

class DX12RenderConfig;

class DX12RenderBackend final : public IRenderBackend
{
public:
    DX12RenderBackend();
    virtual ~DX12RenderBackend() override;

    virtual RendererResult Initialize() override;
    virtual RendererResult Destroy() override;

    virtual const IRenderConfig& GetRenderConfig() const override;

    virtual AsyncComputeBase* GetAsyncCompute() const override;

    virtual Frame* GetCurrentFrame() const override;

    virtual Frame* PrepareNextFrame() override;

    virtual void PrepareSwapchain(Swapchain* swapchain) override;
    virtual void SubmitCommandBuffers(Swapchain* swapchain) override;
    virtual void PresentToSwapchain(Swapchain* swapchain) override;

    virtual CommandBuffer* GetCurrentCommandBuffer() const override;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) override;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef& shader,
        Span<const FramebufferRef> framebuffers,
        const RenderableAttributeSet& attributes) override;

    virtual ComputePipelineRef MakeComputePipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable) override;

    virtual RaytracingPipelineRef MakeRaytracingPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable) override;

    virtual GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) override;

    virtual GpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image) override;
    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers) override;

    virtual SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) override;

    virtual FramebufferRef MakeFramebuffer(Vec2u extent, uint32 numViews = 1) override;
    virtual FramebufferRef MakeFramebuffer(Vec2u extent, RenderTargetType renderTargetType, uint32 numViews = 1) override;

    virtual FrameRef MakeFrame(uint32 frameIndex) override;

    virtual ShaderRef MakeShader(const RC<CompiledShader>& compiledShader) override;

    virtual GpuBlasRef MakeGpuBlas(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;
    virtual GpuTlasRef MakeTLAS() override;

    virtual const GpuImageViewRef& GetTextureImageView(const Handle<Texture>& texture, uint32 mipIndex = 0, uint32 numMips = ~0u, uint32 layerIndex = 0, uint32 numLayers = ~0u) override;

    virtual void PopulateIndirectDrawCommandsBuffer(const GpuBufferRef& vertexBuffer, const GpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer) override;

    virtual TextureFormat GetDefaultFormat(DefaultImageFormat type) const override;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const override;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    virtual void ReleaseTransientMemory() override;

    virtual void NextFrame() override;

private:
    Pimpl<DX12RenderConfig>                         m_renderConfig;

    FixedArray<FrameRef, NumFramesInFlight>         m_frames;
    uint32                                          m_currentFrameIndex;

    FixedArray<CommandBufferRef, NumFramesInFlight> m_commandBuffers;
};

} // namespace Hyperion
