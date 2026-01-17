/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl.h>

#include <core/memory/Pimpl.hpp>

namespace Hyperion {

class DX12RenderConfig;

class DX12RenderBackend final : public IRenderBackend
{
public:
    DX12RenderBackend();
    ~DX12RenderBackend() override;

    RendererResult Initialize() override;
    RendererResult Destroy() override;

    const IRenderConfig& GetRenderConfig() const override;

    AsyncComputeBase* GetAsyncCompute() const override;

    DX12Frame* GetCurrentFrame() const override;

    DX12Frame* PrepareNextFrame() override;
    
    DX12SwapchainRef CreateSwapchain(ApplicationWindow* window) override;

    void PrepareSwapchain(DX12Swapchain* swapchain) override;
    void SubmitCommandBuffers(DX12Swapchain* swapchain) override;
    void PresentToSwapchain(DX12Swapchain* swapchain) override;

    DX12CommandBuffer* GetCurrentCommandBuffer() const override;

    DX12DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    DX12DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) override;

    DX12GraphicsPipelineRef MakeGraphicsPipeline(
        const DX12ShaderRef& shader,
        const RenderTargetDesc& renderTargetDesc,
        const RenderableAttributeSet& attributes) override;

    DX12ComputePipelineRef MakeComputePipeline(
        const DX12ShaderRef& shader,
        const DX12DescriptorTableRef& descriptorTable) override;

    DX12RaytracingPipelineRef MakeRaytracingPipeline(
        const DX12ShaderRef& shader,
        const DX12DescriptorTableRef& descriptorTable) override;

    DX12GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) override;

    DX12GpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image) override;
    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers) override;

    DX12SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) override;

    DX12FramebufferRef MakeFramebuffer(const RenderTargetDesc& renderTargetDesc) override;

    DX12FrameRef MakeFrame(uint32 frameIndex) override;

    DX12ShaderRef MakeShader(const RC<CompiledShader>& compiledShader) override;

    DX12GpuBlasRef MakeGpuBlas(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;
    DX12GpuTlasRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(const DX12GpuBufferRef& vertexBuffer, const DX12GpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer) override;

    TextureFormat GetDefaultFormat(DefaultImageFormat type) const override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const override;

    UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    void ReleaseTransientMemory() override;

    void NextFrame() override;

private:
    Pimpl<DX12RenderConfig> m_renderConfig;

    FixedArray<DX12FrameRef, NumFramesInFlight> m_frames;
    uint32 m_currentFrameIndex;

    FixedArray<DX12CommandBufferRef, NumFramesInFlight> m_commandBuffers;
    
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_directQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_computeQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_copyQueue;
};

} // namespace Hyperion
