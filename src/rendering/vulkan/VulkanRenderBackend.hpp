/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>

#include <rendering/CrashHandler.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/Pimpl.hpp>

#include <vulkan/vulkan.h>
#if defined(HYP_WINDOWS)
#include <vulkan/vulkan_win32.h>
#elif defined(HYP_MACOS)
#include <vulkan/vulkan_metal.h>
#elif defined(HYP_LINUX)
#include <vulkan/vulkan_xlib.h>
#endif

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

#ifdef HYP_DEBUG_MODE
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
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    virtual ~IDummyVulkanSurfaceContext() = default;
};

class VulkanRenderBackend final : public IRenderBackend
{
public:
    VulkanRenderBackend();
    ~VulkanRenderBackend() override;

    HYP_FORCE_INLINE VulkanInstance* GetInstance() const
    {
        return m_instance;
    }

    const VulkanDeviceRef& GetDevice() const;

    RendererResult Initialize() override;
    RendererResult Destroy() override;

    const IRenderConfig& GetRenderConfig() const override;

    AsyncComputeBase* GetAsyncCompute() const override;

    VulkanFrame* GetCurrentFrame() const override;

    VulkanFrame* PrepareNextFrame() override;

    VulkanSwapchainRef CreateSwapchain(ApplicationWindow* window);
    
    void PrepareSwapchain(VulkanSwapchain* swapchain) override;
    void SubmitCommandBuffers(VulkanSwapchain* swapchain) override;
    void PresentToSwapchain(VulkanSwapchain* swapchain) override;

    VulkanCommandBuffer* GetCurrentCommandBuffer() const override;

    VulkanDescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    VulkanDescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) override;

    VulkanGraphicsPipelineRef MakeGraphicsPipeline(
        const VulkanShaderRef& shader,
        const RenderTargetDesc& renderTargetDesc,
        const RenderableAttributeSet& attributes) override;

    VulkanComputePipelineRef MakeComputePipeline(const VulkanShaderRef& shader) override;

    VulkanRayTracingPipelineRef MakeRayTracingPipeline(
        const VulkanShaderRef& shader,
        const VulkanDescriptorTableRef& descriptorTable) override;

    VulkanGpuBufferRef MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment = 0) override;

    VulkanGpuImageRef MakeImage(const TextureDesc& textureDesc) override;

    VulkanGpuImageViewRef MakeImageView(const VulkanGpuImageRef& image) override;
    VulkanGpuImageViewRef MakeImageView(const VulkanGpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers) override;

    VulkanSamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) override;

    VulkanFramebufferRef MakeFramebuffer(const RenderTargetDesc& renderTargetDesc) override;

    VulkanFrameRef MakeFrame(uint32 frameIndex) override;

    VulkanShaderRef MakeShader(const CompiledShader* compiledShader) override;

    VulkanGpuBlasRef MakeGpuBlas(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform) override;
    VulkanGpuTlasRef MakeTLAS() override;

    void PopulateIndirectDrawCommandsBuffer(const VulkanGpuBufferRef& vertexBuffer, const VulkanGpuBufferRef& indexBuffer, uint32 instanceOffset, TByteBuffer<RenderAllocator>& outByteBuffer) override;

    TextureFormat GetDefaultFormat(DefaultImageFormat type) const override;

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const override;

    UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    void ReleaseTransientMemory() override;

    void NextFrame() override
    {
        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumFramesInFlight;
    }

    HYP_API RendererResult CreateDescriptorSet(VkDescriptorSetLayout vkDescriptorSetLayout, VkDescriptorSet& outVkDescriptorSet, VkDescriptorPool& outVkDescriptorPool);

    HYP_API RendererResult DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool);

    HYP_API RendererResult GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VkDescriptorSetLayout& out);

    /*! \brief Create a VkSurfaceKHR for the given window - Window can be null to create a headless surface */
    VkSurfaceKHR CreateSurface(ApplicationWindow* window, IDummyVulkanSurfaceContext** ppOutDummySurfaceContext = nullptr);

    RendererResult GetVkExtensions(Array<const char*>& outExtensions);

private:
    VulkanInstance* m_instance;

    Pimpl<VulkanRenderConfig> m_renderConfig;

    CrashHandler m_crashHandler;

    Pimpl<VulkanDescriptorSetManager> m_descriptorSetManager;

    VulkanAsyncCompute* m_asyncCompute;

    HashMap<DefaultImageFormat, TextureFormat> m_defaultFormats;

    Pimpl<VulkanTextureCache> m_textureCache;

    FixedArray<VulkanFrameRef, NumFramesInFlight> m_frames;
    uint32 m_currentFrameIndex;

    FixedArray<VulkanCommandBufferRef, NumFramesInFlight> m_commandBuffers;
};

} // namespace Hyperion

