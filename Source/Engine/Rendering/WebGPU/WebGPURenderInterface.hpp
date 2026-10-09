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

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUPipelineCompiler.hpp>
#include <Rendering/WebGPU/WebGPUPipelineManifest.hpp>

#include <Core/Memory/Pimpl.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>

namespace Hyperion {

class WebGPURenderConfig;
class WebGPUAsyncCompute;
class WebGPUGpuTimerBackend;
struct WebGPUTransientCommandBuffer;

struct WebGPUDeviceFeatures
{
    bool depthClipControl = false;
    bool float32Filterable = false;
    bool depth32FloatStencil8 = false;
    bool textureFormatsTier1 = false;
    bool indirectFirstInstance = false;
    bool rg11b10UfloatRenderable = false;
};

class WebGPURenderInterface final : public RenderInterface
{
public:
    WebGPURenderInterface();
    ~WebGPURenderInterface() override;

    HYP_FORCE_INLINE WGPUInstance GetInstance() const
    {
        return m_instance;
    }

    HYP_FORCE_INLINE WGPUAdapter GetAdapter() const
    {
        return m_adapter;
    }

    HYP_FORCE_INLINE WGPUDevice GetDevice() const
    {
        return m_device;
    }

    HYP_FORCE_INLINE WGPUQueue GetQueue() const
    {
        return m_queue;
    }

    HYP_FORCE_INLINE const WebGPUDeviceFeatures& GetDeviceFeatures() const
    {
        return m_deviceFeatures;
    }

    HYP_FORCE_INLINE const WGPULimits& GetDeviceLimits() const
    {
        return m_deviceLimits;
    }

    RendererResult Initialize() override;
    void Shutdown() override;

    const IRenderConfig& GetRenderConfig() const override;

    WebGPUFrame* GetCurrentFrame() const override;

    bool CheckDeviceRemoved() const override;

    WebGPUSwapchainRef CreateSwapchain(ApplicationWindow* window, const Vec2u& extent) override;

    void PrepareSwapchain(WebGPUSwapchain* swapchain) override;
    void PresentToSwapchain(WebGPUSwapchain* swapchain) override;

    WebGPUCommandBuffer* GetCurrentCommandBuffer() const override
    {
        return m_commandBuffers[GetFrameCounter() % NumFramesInFlight].Get();
    }

    WebGPUCommandBuffer& GetTransientCommandBuffer() override;
    void SubmitTransientCommandBuffer(WebGPUCommandBuffer& commandBuffer) override;

    WebGPUDescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    WebGPUDescriptorTableRef MakeDescriptorTable(const ShaderInputGroup* decl) override;

    WebGPUGraphicsPipelineRef MakeGraphicsPipeline(
        const WebGPUShaderInstanceRef& shaderInstance,
        const FramebufferDesc& framebufferDesc,
        const RenderableAttributeSet& attributes,
        uint8 stencilWriteMask,
        uint8 stencilCompareMask) override;

    WebGPUComputePipelineRef MakeComputePipeline(const WebGPUShaderInstanceRef& shaderInstance) override;

    WebGPURayTracingPipelineRef MakeRayTracingPipeline(const WebGPUShaderInstanceRef& shaderInstance) override;

    WebGPUGpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment = 0) override;

    WebGPUGpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    WebGPUGpuImageViewRef MakeImageView(const WebGPUGpuImageRef& image) override;
    WebGPUGpuImageViewRef MakeImageView(
        const WebGPUGpuImageRef& image,
        uint8 mipIndex,
        uint8 numMips,
        uint16 layerIndex,
        uint16 numLayers,
        TextureType viewType = TextureType::Max) override;

    WebGPUSamplerRef MakeSampler(const SamplerDesc& samplerDesc) override;

    WebGPUFramebufferRef MakeFramebuffer(const FramebufferDesc& framebufferDesc) override;

    WebGPUFrameRef MakeFrame(uint32 frameIndex) override;

    WebGPUShaderInstanceRef MakeShader(const Shader* shader) override;

    WebGPUBottomLevelASRef MakeBottomLevelAS(
        const WebGPUGpuBufferRef& packedVerticesBuffer,
        const WebGPUGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;

    WebGPUTopLevelASRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(
        const WebGPUGpuBuffer* vertexBuffer,
        const WebGPUGpuBuffer* indexBuffer,
        uint32 numIndices,
        uint32 instanceOffset,
        Array<IndirectDrawCommand, WebGPUAllocator>& outBuffer) override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    HYP_NODISCARD WebGPUAsyncCompute* CreateAsyncCompute() override;
    void SubmitAsyncCompute(WebGPUAsyncCompute* asyncCompute) override;

    void RecordStartTimestamp(WebGPUCommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void RecordStopTimestamp(WebGPUCommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void ResolveGpuFrameResults(uint32 completedFrameIndex) override;

    UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    void ReleaseTransientMemory() override;

    void RegisterDirtyBuffer(WebGPUGpuBuffer* buffer);
    void UnregisterDirtyBuffer(WebGPUGpuBuffer* buffer);

    void Submit(WebGPUCommandBuffer& commandBuffer);

    void ProcessEvents();
    void WaitForEvents();

    HYP_FORCE_INLINE bool IsDeviceLost() const
    {
        return m_isDeviceLost;
    }

    HYP_FORCE_INLINE WebGPUPipelineCompiler& GetPipelineCompiler()
    {
        return m_pipelineCompiler;
    }

    HYP_FORCE_INLINE WebGPUPipelineManifest& GetPipelineManifest()
    {
        return m_pipelineManifest;
    }

    uint32 GetNumPipelinesCompilingInBackground() const override
    {
        return m_pipelineCompiler.GetNumBackgroundCompiles();
    }

    void OnAdapterRequestEnded(WGPUAdapter adapter);
    void OnDeviceRequestEnded(WGPUDevice device);

    void WaitForSubmittedWork();

    WGPUBindGroupLayout GetOrCreateBindGroupLayout(const WGPUBindGroupLayoutEntry* entries, uint32 numEntries);
    WGPUBindGroupLayout GetEmptyBindGroupLayout();
    bool GetBindGroupLayoutEntries(WGPUBindGroupLayout layout, Array<WGPUBindGroupLayoutEntry, WebGPUAllocator>& outEntries);

    void GetStorageTextureOverrides(WGPUBindGroupLayout layout, uint32 group, Array<WebGPUStorageTextureOverride, WebGPUAllocator>& outOverrides);
    WGPUBindGroup GetEmptyBindGroup();

    HYP_FORCE_INLINE WGPUBuffer GetRealignBuffer() const
    {
        return m_realignBuffer;
    }

    bool AllocateRealignedRange(uint32 size, uint32& outOffset);

    WGPUTextureView GetFallbackTextureView(WGPUTextureViewDimension dimension, WGPUTextureSampleType sampleType);

private:
    void InitDeviceDetails(DeviceDetails& deviceDetails) override;

    RendererResult FinishInitialize();

    struct FrameSlot
    {
        AtomicVar<bool> isWorkDone { true };
        Array<WebGPUGpuBufferRef, WebGPUAllocator> readbacks;
    };

    bool IsFrameSlotComplete(const FrameSlot& frameSlot) const;
    void WaitForFrameSlot(FrameSlot& frameSlot);

    void PrepareFrame(WebGPUFrame* frame) override;

    void UploadDirtyBuffers();

    Pimpl<WebGPURenderConfig> m_renderConfig;

    WGPUInstance m_instance;
    WGPUAdapter m_adapter;
    WGPUDevice m_device;
    WGPUQueue m_queue;

    WebGPUDeviceFeatures m_deviceFeatures;
    WGPULimits m_deviceLimits;

    volatile bool m_isDeviceLost;

    enum class InitializeState : uint8
    {
        NotStarted,
        WaitingForAdapter,
        WaitingForDevice,
        DeviceReady,
        Failed
    };

    InitializeState m_initializeState;

    FixedArray<FrameSlot, NumFramesInFlight> m_frameSlots;

    FixedArray<WebGPUFrameRef, NumFramesInFlight> m_frames;
    FixedArray<WebGPUCommandBufferRef, NumFramesInFlight> m_commandBuffers;

    List<WebGPUTransientCommandBuffer, WebGPUAllocator> m_transientCommandBufferStorage;
    Array<WebGPUTransientCommandBuffer*, WebGPUAllocator> m_freeTransientCommandBuffers;
    Array<WebGPUTransientCommandBuffer*, WebGPUAllocator> m_recordingTransientCommandBuffers;
    Mutex m_transientCommandBuffersMutex;

    Array<WebGPUAsyncCompute*, WebGPUAllocator> m_asyncComputePool;
    Array<WebGPUAsyncCompute*, WebGPUAllocator> m_submittedAsyncComputes;
    Mutex m_asyncComputesMutex;

    Array<WebGPUGpuBuffer*, WebGPUAllocator> m_dirtyBuffers;
    Mutex m_dirtyBuffersMutex;

    Mutex m_submitMutex;

    WGPUBuffer m_realignBuffer;
    uint32 m_realignBufferCursor;

    WebGPUPipelineCompiler m_pipelineCompiler;
    WebGPUPipelineManifest m_pipelineManifest;
    Mutex m_realignBufferMutex;

    Map<uint64, WGPUBindGroupLayout, WebGPUAllocator> m_bindGroupLayouts;
    Map<WGPUBindGroupLayout, Array<WebGPUStorageTextureOverride, WebGPUAllocator>, WebGPUAllocator> m_storageTextureEntries;
    Map<WGPUBindGroupLayout, Array<WGPUBindGroupLayoutEntry, WebGPUAllocator>, WebGPUAllocator> m_bindGroupLayoutEntries;
    Mutex m_bindGroupLayoutsMutex;

    WGPUBindGroup m_emptyBindGroup;

    struct FallbackTexture
    {
        WGPUTextureViewDimension dimension;
        WGPUTextureSampleType sampleType;
        WGPUTexture texture;
        WGPUTextureView view;
    };

    Array<FallbackTexture, WebGPUAllocator> m_fallbackTextures;
    Mutex m_fallbackTexturesMutex;
};

} // namespace Hyperion
