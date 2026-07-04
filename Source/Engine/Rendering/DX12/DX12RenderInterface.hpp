/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/RenderInterface.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/DX12/DX12GpuBuffer.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>

#include <Rendering/DX12/DX12Shared.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <atomic>

#include <D3D12MemAlloc.h>

#include <dxgi1_6.h>

namespace Hyperion {

class DX12RenderConfig;
class DX12DescriptorHeapManager;
class DX12AsyncCompute;
class DX12Fence;
class DX12GpuTimerBackend;

struct DX12QueueData
{
    ComPtr<ID3D12CommandQueue> commandQueue;
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
        if (HYP_UNLIKELY(commandListType) >= m_queueData.Size())
        {
            return nullptr;
        }

        return &m_queueData[uint32(commandListType)];
    }

    HYP_FORCE_INLINE D3D12MA::Allocator* GetAllocator() const
    {
        return m_allocator;
    }

    RendererResult Initialize() override;
    void Shutdown() override;

    const IRenderConfig& GetRenderConfig() const override;

    DX12Frame* GetCurrentFrame() const override;

    DX12SwapchainRef CreateSwapchain(ApplicationWindow* window, const Vec2u& extent) override;

    void PrepareSwapchain(DX12Swapchain* swapchain) override;
    void PresentToSwapchain(DX12Swapchain* swapchain) override;

    DX12CommandBuffer* GetCurrentCommandBuffer() const override
    {
        return m_commandBuffers[GetFrameCounter() % NumFramesInFlight].Get();
    }

    HYP_FORCE_INLINE uint32 GetCurrentFrameIndex() const
    {
        return GetFrameCounter() % NumFramesInFlight;
    }

    HYP_FORCE_INLINE ID3D12Fence* GetFrameFence() const
    {
        return m_frameFence.Get();
    }

    /*! \brief Checks if the D3D12 device has been removed and logs the reason.
     *  \return true if the device has been removed, false otherwise. */
    bool CheckDeviceRemoved() const;

    DX12CommandBuffer& GetTransientCommandBuffer() override;
    void SubmitTransientCommandBuffer(DX12CommandBuffer& commandBuffer) override;

    DX12DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    DX12DescriptorTableRef MakeDescriptorTable(const ShaderInputGroup* decl) override;

    DX12GraphicsPipelineRef MakeGraphicsPipeline(
        const DX12ShaderInstanceRef& shaderInstance,
        const FramebufferDesc& framebufferDesc,
        const RenderableAttributeSet& attributes,
        uint8 stencilWriteMask,
        uint8 stencilCompareMask) override;

    DX12ComputePipelineRef MakeComputePipeline(const DX12ShaderInstanceRef& shaderInstance) override;

    DX12RayTracingPipelineRef MakeRayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance) override;

    DX12GpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment = 0) override;

    DX12GpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    DX12GpuImageViewRef MakeImageView(const DX12GpuImageRef& image) override;
    DX12GpuImageViewRef MakeImageView(
        const DX12GpuImageRef& image,
        uint8 mipIndex,
        uint8 numMips,
        uint16 layerIndex,
        uint16 numLayers,
        TextureType viewType = TextureType::Max) override;

    DX12SamplerRef MakeSampler(const SamplerDesc& samplerDesc) override;

    DX12FramebufferRef MakeFramebuffer(const FramebufferDesc& framebufferDesc) override;

    DX12FrameRef MakeFrame(uint32 frameIndex) override;

    DX12ShaderInstanceRef MakeShader(const Shader* shader) override;

    DX12BottomLevelASRef MakeBottomLevelAS(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;
    DX12TopLevelASRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(
        const DX12GpuBuffer* vertexBuffer,
        const DX12GpuBuffer* indexBuffer,
        uint32 instanceOffset,
        Array<D3D12_DRAW_INDEXED_ARGUMENTS, DX12Allocator>& outBuffer) override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    HYP_NODISCARD DX12AsyncCompute* CreateAsyncCompute() override;
    void SubmitAsyncCompute(DX12AsyncCompute* asyncCompute) override;

    void RecordStartTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void RecordStopTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void ResolveGpuFrameResults(uint32 completedFrameIndex) override;

    UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    void ReleaseTransientMemory() override;

    void BeginFrame(AtomicFlag* pCancelFlag) override;

    void InsertTransientSyncBarrier();

    ComPtr<IDXGIFactory4> dxgiFactory;

    DX12DescriptorHeapManager* descriptorHeapManager;

private:
    void InitDeviceDetails(DeviceDetails& deviceDetails) override;

    void BindDescriptorHeaps(DX12CommandBuffer& commandBuffer);

    void PrepareFrame(DX12Frame* frame) override;

    Pimpl<DX12RenderConfig> m_renderConfig;

    FixedArray<DX12FrameRef, NumFramesInFlight> m_frames;

    FixedArray<DX12CommandBufferRef, NumFramesInFlight> m_commandBuffers;

    List<DX12CommandBuffer, DX12Allocator> m_transientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];
    List<DX12CommandBuffer, DX12Allocator> m_pendingTransientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];

    List<DX12Fence, DX12Allocator> m_transientCommandBufferFences[NumFramesInFlight];
    List<DX12Fence, DX12Allocator> m_recycledTransientCommandBufferFences;
    Mutex m_transientCommandBuffersMutex;

    ComPtr<ID3D12Fence> m_transientSyncFence;
    AtomicVar<uint64> m_transientSyncValues[NumFramesInFlight];

    ComPtr<IDXGIAdapter1> m_hardwareAdapter;

    ComPtr<ID3D12Device> m_device;

    FixedArray<DX12QueueData, 4> m_queueData;

    ComPtr<ID3D12Fence> m_frameFence;
    HANDLE m_frameFenceEvent;

    FixedArray<uint64, NumFramesInFlight> m_frameFenceValues;
    uint8 m_frameFenceIndex;

    FixedArray<int64, NumFramesInFlight> m_submissionFrames;

    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> m_dredSettings;

    D3D12MA::Allocator* m_allocator;

    Array<DX12AsyncCompute*, DX12Allocator> m_asyncComputePool;
    Array<DX12AsyncCompute*, DX12Allocator> m_submittedAsyncComputes;
    Mutex m_asyncComputesMutex;
};

} // namespace Hyperion
