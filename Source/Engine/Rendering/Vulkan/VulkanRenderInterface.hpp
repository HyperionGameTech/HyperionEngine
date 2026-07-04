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

#include <Rendering/Vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/Vulkan/VulkanGpuImage.hpp>
#include <Rendering/Vulkan/VulkanGpuTimerBackend.hpp>

#include <Rendering/Shared.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Memory/SharedPtr.hpp>
#include <Core/Memory/Pimpl.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanInstance;
class VulkanAsyncCompute;
class VulkanRenderConfig;

class VulkanDescriptorSetLayoutWrapper;

class VulkanDescriptorSetManager;

struct VulkanDynamicFunctions
{
    void Load(VulkanDevice* device);

#define HYP_DECL_FN(name) PFN_##name name = nullptr

    // ray tracing requirements
    HYP_DECL_FN(vkGetBufferDeviceAddressKHR);
    HYP_DECL_FN(vkCmdBuildAccelerationStructuresKHR);
    HYP_DECL_FN(vkBuildAccelerationStructuresKHR);
    HYP_DECL_FN(vkCreateAccelerationStructureKHR);
    HYP_DECL_FN(vkDestroyAccelerationStructureKHR);
    HYP_DECL_FN(vkGetAccelerationStructureBuildSizesKHR);
    HYP_DECL_FN(vkGetAccelerationStructureDeviceAddressKHR);
    HYP_DECL_FN(vkCmdTraceRaysKHR);
    HYP_DECL_FN(vkGetRayTracingShaderGroupHandlesKHR);
    HYP_DECL_FN(vkCreateRayTracingPipelinesKHR);

    // timeline semaphores
    HYP_DECL_FN(vkSignalSemaphore);
    HYP_DECL_FN(vkWaitSemaphores);
    HYP_DECL_FN(vkGetSemaphoreCounterValue);

#if HYP_DEBUG_MODE
    // debugging
    HYP_DECL_FN(vkCmdDebugMarkerBeginEXT);
    HYP_DECL_FN(vkCmdDebugMarkerEndEXT);
    HYP_DECL_FN(vkCmdDebugMarkerInsertEXT);
#endif

#ifdef HYP_RHI_DEBUG_NAMES
    HYP_DECL_FN(vkDebugMarkerSetObjectNameEXT);
    HYP_DECL_FN(vkSetDebugUtilsObjectNameEXT);
    HYP_DECL_FN(vkSetDebugUtilsObjectTagEXT);
#endif

    // extended dynamic state (VK_EXT_extended_dynamic_state)
    HYP_DECL_FN(vkCmdSetDepthWriteEnableEXT);
    HYP_DECL_FN(vkCmdSetDepthTestEnableEXT);
    HYP_DECL_FN(vkCmdSetDepthCompareOpEXT);

#undef HYP_DECL_FN
};

class IDummyVulkanSurfaceContext
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_vulkanPool);

    virtual ~IDummyVulkanSurfaceContext() = default;
};

class VulkanRenderInterface final : public RenderInterface
{
public:
    VulkanRenderInterface();
    ~VulkanRenderInterface() override;

    HYP_FORCE_INLINE VulkanInstance* GetInstance() const
    {
        return m_instance;
    }

    const VulkanDeviceRef& GetDevice() const;

    RendererResult Initialize() override;
    void Shutdown() override;

    const IRenderConfig& GetRenderConfig() const override;

    VulkanFrame* GetCurrentFrame() const override;

    VulkanSwapchainRef CreateSwapchain(ApplicationWindow* window, const Vec2u& extent) override;

    void PrepareSwapchain(VulkanSwapchain* swapchain) override;
    void PresentToSwapchain(VulkanSwapchain* swapchain) override;

    void BeginFrame(AtomicFlag* pCancelFlag) override;
    void EndFrame() override;

    VulkanCommandBuffer* GetCurrentCommandBuffer() const override;

    VulkanCommandBuffer& GetTransientCommandBuffer() override;
    void SubmitTransientCommandBuffer(VulkanCommandBuffer& commandBuffer) override;

    VulkanDescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    VulkanDescriptorTableRef MakeDescriptorTable(const ShaderInputGroup* decl) override;

    VulkanGraphicsPipelineRef MakeGraphicsPipeline(
        const VulkanShaderInstanceRef& shaderInstance,
        const FramebufferDesc& framebufferDesc,
        const RenderableAttributeSet& attributes,
        uint8 stencilWriteMask,
        uint8 stencilCompareMask) override;

    VulkanComputePipelineRef MakeComputePipeline(const VulkanShaderInstanceRef& shaderInstance) override;

    VulkanRayTracingPipelineRef MakeRayTracingPipeline(const VulkanShaderInstanceRef& shaderInstance) override;

    VulkanGpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment = 0) override;

    VulkanGpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    VulkanGpuImageViewRef MakeImageView(const VulkanGpuImageRef& image) override;
    VulkanGpuImageViewRef MakeImageView(
        const VulkanGpuImageRef& image,
        uint8 mipIndex,
        uint8 numMips,
        uint16 layerIndex,
        uint16 numLayers,
        TextureType viewType = TextureType::Max) override;

    VulkanSamplerRef MakeSampler(const SamplerDesc& samplerDesc) override;

    VulkanFramebufferRef MakeFramebuffer(const FramebufferDesc& framebufferDesc) override;

    VulkanFrameRef MakeFrame(uint32 frameIndex) override;

    VulkanShaderInstanceRef MakeShader(const Shader* shader) override;

    VulkanBottomLevelASRef MakeBottomLevelAS(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;

    VulkanTopLevelASRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(
        const VulkanGpuBuffer* vertexBuffer,
        const VulkanGpuBuffer* indexBuffer,
        uint32 instanceOffset,
        Array<VkDrawIndexedIndirectCommand, VulkanAllocator>& outBuffer) override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    HYP_NODISCARD VulkanAsyncCompute* CreateAsyncCompute() override;
    void SubmitAsyncCompute(VulkanAsyncCompute* asyncCompute) override;

    void RecordStartTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void RecordStopTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void ResolveGpuFrameResults(uint32 completedFrameIndex) override;

    RendererResult CreateDescriptorSet(
        VkDescriptorSetLayout vkDescriptorSetLayout,
        bool isBindlessTextures, bool isBindlessBuffers, bool isRayTracing,
        VkDescriptorSet& outVkDescriptorSet,
        VkDescriptorPool& outVkDescriptorPool);

    RendererResult DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool);

    RendererResult GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VkDescriptorSetLayout& out);

    /*! \brief Create a VkSurfaceKHR for the given window - Window can be null to create a headless surface */
    VkSurfaceKHR CreateSurface(ApplicationWindow* window, IDummyVulkanSurfaceContext** ppOutDummySurfaceContext = nullptr);

    RendererResult GetVkExtensions(Array<const char*>& outExtensions);

    VulkanDynamicFunctions dynamicFunctions;

private:
    void InitDeviceDetails(DeviceDetails& deviceDetails) override;

    void ReleaseTransientMemory() override;

    void NewFrameIndex() override
    {
        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumFramesInFlight;
    }

    void PrepareFrame(VulkanFrame* frame) override;

    VulkanInstance* m_instance;

    VulkanRenderConfig* m_renderConfig;

    VulkanDescriptorSetManager* m_descriptorSetManager;

    Array<VulkanFrameRef, VulkanAllocator> m_frames;
    uint32 m_currentFrameIndex;

    Array<VulkanCommandBufferRef, VulkanAllocator> m_commandBuffers;

    List<VulkanCommandBuffer, VulkanAllocator> m_transientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];
    List<VulkanCommandBuffer, VulkanAllocator> m_pendingTransientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];

    List<VulkanSemaphore, VulkanAllocator> m_transientCommandBufferSemaphores[NumFramesInFlight];
    List<VulkanSemaphore, VulkanAllocator> m_recycledTransientCommandBufferSemaphores;

    List<VulkanFence, VulkanAllocator> m_transientCommandBufferFences[NumFramesInFlight];
    List<VulkanFence, VulkanAllocator> m_recycledTransientCommandBufferFences;

    Mutex m_transientCommandBuffersMutex;

    Array<VulkanAsyncCompute*, VulkanAllocator> m_asyncComputePool;
    Array<VulkanAsyncCompute*, VulkanAllocator> m_submittedAsyncComputes;
    Mutex m_asyncComputesMutex;
};

} // namespace Hyperion
