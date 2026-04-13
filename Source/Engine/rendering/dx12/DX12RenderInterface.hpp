/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderInterface.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>

#include <rendering/dx12/DX12Shared.hpp>

#include <Core/memory/Pimpl.hpp>

#include <D3D12MemAlloc.h>

#include <dxgi1_6.h>

namespace Hyperion {

class DX12RenderConfig;
class DX12DescriptorHeapManager;
class DX12AsyncCompute;
class DX12Fence;

struct DX12QueueData
{
    ComPtr<ID3D12CommandQueue> commandQueue;
    FixedArray<ComPtr<ID3D12CommandAllocator>, NumFramesInFlight> commandAllocators;
};

class DX12RenderInterface final : public RenderInterface
{
public:
    DX12RenderInterface();
    ~DX12RenderInterface() override;

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
    void Shutdown() override;

    const IRenderConfig& GetRenderConfig() const override;

    DX12Frame* GetCurrentFrame() const override;

    DX12Frame* PrepareNextFrame() override;
    
    DX12SwapchainRef CreateSwapchain(ApplicationWindow* window, const Vec2u& extent) override;

    void PrepareSwapchain(DX12Swapchain* swapchain) override;
    void SubmitCommandBuffers(DX12Swapchain* swapchain) override;
    void PresentToSwapchain(DX12Swapchain* swapchain) override;

    DX12CommandBuffer* GetCurrentCommandBuffer() const override;

    DX12CommandBuffer& GetTransientCommandBuffer() override;
    void SubmitTransientCommandBuffer(DX12CommandBuffer& commandBuffer) override;

    DX12DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    DX12DescriptorTableRef MakeDescriptorTable(const ShaderInputGroup* decl) override;

    DX12GraphicsPipelineRef MakeGraphicsPipeline(
        const DX12ShaderInstanceRef& shaderInstance,
        const FramebufferDesc& framebufferDesc,
        const RenderableAttributeSet& attributes) override;

    DX12ComputePipelineRef MakeComputePipeline(const DX12ShaderInstanceRef& shaderInstance) override;

    DX12RayTracingPipelineRef MakeRayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance) override;

    DX12GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment = 0) override;

    DX12GpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image) override;
    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image, uint8 mipIndex, uint8 numMips, uint16 layerIndex, uint16 numLayers) override;

    DX12SamplerRef MakeSampler(const SamplerDesc& samplerDesc) override;

    DX12FramebufferRef MakeFramebuffer(const FramebufferDesc& framebufferDesc) override;

    DX12FrameRef MakeFrame(uint32 frameIndex) override;

    DX12ShaderInstanceRef MakeShader(const Shader* shader) override;

    DX12GpuBlasRef MakeGpuBlas(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;
    DX12GpuTlasRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(const DX12GpuBufferRef& vertexBuffer, const DX12GpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer) override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;
    
    HYP_NODISCARD DX12AsyncCompute* CreateAsyncCompute() override;
    void SubmitAsyncCompute(DX12AsyncCompute* asyncCompute) override;

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

    Array<DX12CommandBuffer*, RenderAllocator> m_transientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];
    Array<DX12CommandBuffer*, RenderAllocator> m_pendingTransientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];

    Array<DX12Fence*, RenderAllocator> m_transientCommandBufferFences[NumRendererWorkerThreads + 1][NumFramesInFlight];
    Array<DX12Fence*, RenderAllocator> m_recycledTransientCommandBufferFences;

    Array<DX12CommandBuffer*, RenderAllocator> m_ownedTransientCommandBuffers;
    Array<DX12Fence*, RenderAllocator> m_ownedTransientCommandBufferFences;
    Mutex m_transientCommandBuffersMutex;
    
    ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    ComPtr<ID3D12Device> m_device;

    FlatMap<D3D12_COMMAND_LIST_TYPE, DX12QueueData> m_queueData;

    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> m_dredSettings;

    D3D12MA::Allocator* m_allocator;

    Array<DX12AsyncCompute*, RenderAllocator> m_asyncComputePool;
    Array<DX12AsyncCompute*, RenderAllocator> m_submittedAsyncComputes;
    Mutex m_asyncComputesMutex;
};


} // namespace Hyperion
