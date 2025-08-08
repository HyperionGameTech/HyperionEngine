/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderBackend.hpp>

#include <rendering/CrashHandler.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/Pimpl.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanInstance;
class VulkanAsyncCompute;
class VulkanRenderConfig;

class VulkanDescriptorSetLayoutWrapper;
using VulkanDescriptorSetLayoutWrapperRef = RenderObjectHandle_Strong<VulkanDescriptorSetLayoutWrapper>;
using VulkanDescriptorSetLayoutWrapperWeakRef = RenderObjectHandle_Weak<VulkanDescriptorSetLayoutWrapper>;

class VulkanDescriptorSetManager;

VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout);

class VulkanTextureCache;

struct VulkanDynamicFunctions
{
    static void Load(VulkanDevice* device);

#define HYP_DECL_FN(name) PFN_##name name

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

#undef HYP_DECL_FN
};

HYP_API extern VulkanDynamicFunctions* g_vulkanDynamicFunctions;

class VulkanRenderBackend final : public IRenderBackend
{
public:
    VulkanRenderBackend();
    virtual ~VulkanRenderBackend() override;

    HYP_FORCE_INLINE VulkanInstance* GetInstance() const
    {
        return m_instance;
    }

    const VulkanDeviceRef& GetDevice() const;

    virtual RendererResult Initialize(AppContextBase& appContext) override;
    virtual RendererResult Destroy() override;

    virtual const IRenderConfig& GetRenderConfig() const override;

    virtual SwapchainBase* GetSwapchain() const override;
    virtual AsyncComputeBase* GetAsyncCompute() const override;

    virtual FrameBase* GetCurrentFrame() const override;
    virtual FrameBase* PrepareNextFrame() override;
    virtual void PresentFrame(FrameBase* frame) override;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout& layout) override;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration* decl) override;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
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
    virtual GpuImageViewRef MakeImageView(const GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 faceIndex, uint32 numFaces) override;

    virtual SamplerRef MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode) override;

    virtual FramebufferRef MakeFramebuffer(Vec2u extent, uint32 numViews = 1) override;
    virtual FramebufferRef MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 numViews = 1) override;

    virtual FrameRef MakeFrame(uint32 frameIndex) override;

    virtual ShaderRef MakeShader(const RC<CompiledShader>& compiledShader) override;

    virtual BLASRef MakeBLAS(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Matrix4& transform) override;
    virtual TLASRef MakeTLAS() override;

    virtual const GpuImageViewRef& GetTextureImageView(const Handle<Texture>& texture, uint32 mipIndex = 0, uint32 numMips = ~0u, uint32 faceIndex = 0, uint32 numFaces = ~0u) override;

    virtual void PopulateIndirectDrawCommandsBuffer(const GpuBufferRef& vertexBuffer, const GpuBufferRef& indexBuffer, uint32 instanceOffset, ByteBuffer& outByteBuffer) override;

    virtual TextureFormat GetDefaultFormat(DefaultImageFormat type) const override;

    virtual bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const override;
    virtual TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const override;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc& textureDesc) const override;

    virtual UniquePtr<SingleTimeCommands> GetSingleTimeCommands() override;

    virtual Delegate<void, SwapchainBase*>& GetOnSwapchainRecreatedDelegate() override
    {
        return OnSwapchainRecreated;
    }

    HYP_API RendererResult CreateDescriptorSet(const VulkanDescriptorSetLayoutWrapperRef& layout, VkDescriptorSet& outVkDescriptorSet);
    HYP_API RendererResult DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet);
    HYP_API RendererResult GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VulkanDescriptorSetLayoutWrapperRef& outRef);

private:
    Delegate<void, SwapchainBase*> OnSwapchainRecreated;

    VulkanInstance* m_instance;

    Pimpl<VulkanRenderConfig> m_renderConfig;

    CrashHandler m_crashHandler;

    Pimpl<VulkanDescriptorSetManager> m_descriptorSetManager;

    Pimpl<VulkanAsyncCompute> m_asyncCompute;

    HashMap<DefaultImageFormat, TextureFormat> m_defaultFormats;

    Pimpl<VulkanTextureCache> m_textureCache;

    bool m_shouldRecreateSwapchain;
};

} // namespace hyperion
