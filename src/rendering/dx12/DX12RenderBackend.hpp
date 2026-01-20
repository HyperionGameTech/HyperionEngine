/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12Shared.hpp>

#include <core/memory/Pimpl.hpp>

#include <D3D12MemAlloc.h>

#include <dxgi1_6.h>

namespace Hyperion {

class DX12RenderConfig;
class DX12DescriptorHeapManager;

struct DX12QueueData
{
    ComPtr<ID3D12CommandQueue> commandQueue;
    FixedArray<ComPtr<ID3D12CommandAllocator>, NumFramesInFlight> commandAllocators;
};

class DX12RenderBackend final : public IRenderBackend
{
public:
    DX12RenderBackend();
    ~DX12RenderBackend() override;

    HYP_FORCE_INLINE ID3D12Device* GetDevice() const
    {
        return m_device.Get();
    }

    HYP_FORCE_INLINE const DX12QueueData* GetQueueData(D3D12_COMMAND_LIST_TYPE commandListType) const
    {
        auto it = m_queueData.Find(commandListType);
        if (it != m_queueData.End())
            return &it->second;
        return nullptr;
    }

    HYP_FORCE_INLINE D3D12MA::Allocator* GetAllocator() const
    {
        return m_allocator;
    }

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

    DX12RayTracingPipelineRef MakeRayTracingPipeline(
        const DX12ShaderRef& shader,
        const DX12DescriptorTableRef& descriptorTable) override;

    DX12GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) override;

    DX12GpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image) override;
    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers) override;

    DX12SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) override;

    DX12FramebufferRef MakeFramebuffer(const RenderTargetDesc& renderTargetDesc) override;

    DX12FrameRef MakeFrame(uint32 frameIndex) override;

    DX12ShaderRef MakeShader(const CompiledShader* compiledShader) override;

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
    
    ComPtr<IDXGIFactory4> dxgiFactory;

    DX12DescriptorHeapManager* descriptorHeapManager;

private:
    Pimpl<DX12RenderConfig> m_renderConfig;

    FixedArray<DX12FrameRef, NumFramesInFlight> m_frames;
    uint32 m_currentFrameIndex;

    DX12CommandBufferRef m_commandBuffer;
    
    ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    ComPtr<ID3D12Device> m_device;

    FlatMap<D3D12_COMMAND_LIST_TYPE, DX12QueueData> m_queueData;

    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> m_dredSettings;

    D3D12MA::Allocator* m_allocator;
};


} // namespace Hyperion
