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

#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/memory/RefCountedPtr.hpp>
#include <Core/memory/Pimpl.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class ApplicationWindow;

class VulkanInstance;
class VulkanAsyncCompute;
class VulkanRenderConfig;

class VulkanDescriptorSetLayoutWrapper;

class VulkanDescriptorSetManager;

class VulkanTextureCache;

struct VulkanDynamicFunctions
{
    static void Load(VulkanDevice* device);

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

#if HYP_DEBUG_MODE
    // debugging
    HYP_DECL_FN(vkCmdDebugMarkerBeginEXT);
    HYP_DECL_FN(vkCmdDebugMarkerEndEXT);
    HYP_DECL_FN(vkCmdDebugMarkerInsertEXT);
    HYP_DECL_FN(vkDebugMarkerSetObjectNameEXT);
    HYP_DECL_FN(vkSetDebugUtilsObjectNameEXT);
#endif

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    HYP_DECL_FN(vkGetMoltenVKConfigurationMVK);
    HYP_DECL_FN(vkSetMoltenVKConfigurationMVK);
#endif

#ifdef HYP_WINDOWS
    HYP_DECL_FN(vkGetMemoryWin32HandleKHR);
#else
    HYP_DECL_FN(vkGetMemoryFdKHR);
#endif

#undef HYP_DECL_FN
};

HYP_API extern VulkanDynamicFunctions* g_vulkanDynamicFunctions;

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

    VulkanFrame* PrepareNextFrame() override;

    VulkanSwapchainRef CreateSwapchain(ApplicationWindow* window, const Vec2u& extent) override;

    void PrepareSwapchain(VulkanSwapchain* swapchain) override;
    void PresentToSwapchain(VulkanSwapchain* swapchain) override;

    VulkanCommandBuffer* GetCurrentCommandBuffer() const override;

    VulkanCommandBuffer& GetTransientCommandBuffer() override;
    void SubmitTransientCommandBuffer(VulkanCommandBuffer& commandBuffer) override;

    VulkanDescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    VulkanDescriptorTableRef MakeDescriptorTable(const ShaderInputGroup* decl) override;

    VulkanGraphicsPipelineRef MakeGraphicsPipeline(
        const VulkanShaderInstanceRef& shaderInstance,
        const FramebufferDesc& framebufferDesc,
        const RenderableAttributeSet& attributes) override;

    VulkanComputePipelineRef MakeComputePipeline(const VulkanShaderInstanceRef& shaderInstance) override;

    VulkanRayTracingPipelineRef MakeRayTracingPipeline(const VulkanShaderInstanceRef& shaderInstance) override;

    VulkanGpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment = 0) override;

    VulkanGpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    VulkanGpuImageViewRef MakeImageView(const VulkanGpuImageRef& image) override;
    VulkanGpuImageViewRef MakeImageView(const VulkanGpuImageRef& image, uint8 mipIndex, uint8 numMips, uint16 layerIndex, uint16 numLayers) override;

    VulkanSamplerRef MakeSampler(const SamplerDesc& samplerDesc) override;

    VulkanFramebufferRef MakeFramebuffer(const FramebufferDesc& framebufferDesc) override;

    VulkanFrameRef MakeFrame(uint32 frameIndex) override;

    VulkanShaderInstanceRef MakeShader(const Shader* shader) override;

    VulkanGpuBlasRef MakeGpuBlas(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<MaterialInstance>& material,
        const Mat4f& transform) override;
    VulkanGpuTlasRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(const VulkanGpuBufferRef& vertexBuffer, const VulkanGpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer) override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    HYP_NODISCARD VulkanAsyncCompute* CreateAsyncCompute() override;
    void SubmitAsyncCompute(VulkanAsyncCompute* asyncCompute) override;

    void ReleaseTransientMemory() override;

    void NextFrame() override
    {
        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumFramesInFlight;
    }

    HYP_API RendererResult CreateDescriptorSet(
        VkDescriptorSetLayout vkDescriptorSetLayout,
        bool isBindlessTextures, bool isBindlessBuffers, bool isRayTracing,
        VkDescriptorSet& outVkDescriptorSet,
        VkDescriptorPool& outVkDescriptorPool);

    HYP_API RendererResult DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool);

    HYP_API RendererResult GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VkDescriptorSetLayout& out);

    /*! \brief Create a VkSurfaceKHR for the given window - Window can be null to create a headless surface */
    VkSurfaceKHR CreateSurface(ApplicationWindow* window, IDummyVulkanSurfaceContext** ppOutDummySurfaceContext = nullptr);

    RendererResult GetVkExtensions(Array<const char*>& outExtensions);

private:
    VulkanInstance* m_instance;

    Pimpl<VulkanRenderConfig> m_renderConfig;

    Pimpl<VulkanDescriptorSetManager> m_descriptorSetManager;

    Pimpl<VulkanTextureCache> m_textureCache;

    Array<VulkanFrameRef, VulkanAllocator> m_frames;
    uint32 m_currentFrameIndex;

    Array<VulkanCommandBufferRef, VulkanAllocator> m_commandBuffers;

    LinkedList<VulkanCommandBuffer, VulkanAllocator> m_transientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];
    LinkedList<VulkanCommandBuffer, VulkanAllocator> m_pendingTransientCommandBuffers[NumRendererWorkerThreads + 1][NumFramesInFlight];

    LinkedList<VulkanFence, VulkanAllocator> m_transientCommandBufferFences[NumFramesInFlight];
    LinkedList<VulkanFence, VulkanAllocator> m_recycledTransientCommandBufferFences;

    Mutex m_transientCommandBuffersMutex;

    Array<VulkanAsyncCompute*, VulkanAllocator> m_asyncComputePool;
    Array<VulkanAsyncCompute*, VulkanAllocator> m_submittedAsyncComputes;
    Mutex m_asyncComputesMutex;
};

} // namespace Hyperion
