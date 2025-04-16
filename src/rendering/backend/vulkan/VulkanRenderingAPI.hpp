/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RENDERING_API_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RENDERING_API_HPP

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/Platform.hpp>

#include <rendering/CrashHandler.hpp>

#include <core/containers/HashMap.hpp>

namespace hyperion {
namespace renderer {
    
namespace platform {

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class Device;

} // namespace platform

struct VulkanDescriptorSetLayoutWrapper;
class VulkanDescriptorSetManager;

extern HYP_API VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper &layout);

class VulkanRenderingAPI final : public IRenderingAPI
{
public:
    VulkanRenderingAPI();
    virtual ~VulkanRenderingAPI() override;

    HYP_FORCE_INLINE platform::Instance<Platform::VULKAN> *GetInstance() const
        { return m_instance; }

    platform::Device<Platform::VULKAN> *GetDevice() const;

    virtual RendererResult Initialize(AppContext &app_context) override;
    virtual RendererResult Destroy() override;

    virtual const IRenderConfig &GetRenderConfig() const override
        { return *m_render_config; }

    virtual SwapchainBase *GetSwapchain() const override;

    virtual AsyncComputeBase *GetAsyncCompute() const override
        { return m_async_compute; }

    virtual FrameBase *GetCurrentFrame() const override;
    virtual FrameBase *PrepareNextFrame() override;
    virtual void PresentFrame(FrameBase *frame) override;

    virtual DescriptorSetRef MakeDescriptorSet(const DescriptorSetLayout &layout) override;

    virtual DescriptorTableRef MakeDescriptorTable(const DescriptorTableDeclaration &decl) override;

    virtual GraphicsPipelineRef MakeGraphicsPipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        const Array<FramebufferRef> &framebuffers,
        const RenderableAttributeSet &attributes
    ) override;

    virtual ComputePipelineRef MakeComputePipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table
    ) override;

    virtual RaytracingPipelineRef MakeRaytracingPipeline(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table
    ) override;

    virtual GPUBufferRef MakeGPUBuffer(GPUBufferType buffer_type, SizeType size, SizeType alignment = 0) override;

    virtual ImageRef MakeImage(const TextureDesc &texture_desc) override;

    virtual ImageViewRef MakeImageView() override;

    virtual SamplerRef MakeSampler(FilterMode filter_mode_min, FilterMode filter_mode_mag, WrapMode wrap_mode) override;

    virtual FramebufferRef MakeFramebuffer(Vec2u extent, uint32 num_views = 1) override;
    virtual FramebufferRef MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 num_views = 1) override;

    virtual FrameRef MakeFrame(uint32 frame_index) override;

    virtual ShaderRef MakeShader(const RC<CompiledShader> &compiled_shader) override;

    virtual BLASRef MakeBLAS(
        const GPUBufferRef &packed_vertices_buffer,
        const GPUBufferRef &packed_indices_buffer,
        const Handle<Material> &material,
        const Matrix4 &transform
    ) override;
    virtual TLASRef MakeTLAS() override;

    virtual InternalFormat GetDefaultFormat(DefaultImageFormatType type) const override;

    virtual bool IsSupportedFormat(InternalFormat format, ImageSupportType support_type) const override;
    virtual InternalFormat FindSupportedFormat(Span<InternalFormat> possible_formats, ImageSupportType support_type) const override;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc &texture_desc) const override;

    virtual Delegate<void, SwapchainBase *> &GetOnSwapchainRecreatedDelegate() override
        { return OnSwapchainRecreated; }

    RendererResult CreateDescriptorSet(const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set);
    RendererResult DestroyDescriptorSet(VkDescriptorSet vk_descriptor_set);
    RC<VulkanDescriptorSetLayoutWrapper> GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout &layout);

private:
    Delegate<void, SwapchainBase *>                             OnSwapchainRecreated;

    platform::Instance<Platform::VULKAN>                        *m_instance;

    IRenderConfig                                               *m_render_config;

    CrashHandler                                                m_crash_handler;

    VulkanDescriptorSetManager                                  *m_descriptor_set_manager;

    AsyncComputeBase                                            *m_async_compute;

    HashMap<DefaultImageFormatType, InternalFormat>             m_default_formats;

    bool                                                        m_should_recreate_swapchain;
};

} // namespace renderer
} // namespace hyperion

#endif

