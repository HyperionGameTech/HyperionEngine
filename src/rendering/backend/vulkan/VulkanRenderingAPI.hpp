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

template <PlatformType PLATFORM>
class DescriptorSetLayout;

} // namespace platform

struct VulkanDescriptorSetLayoutWrapper;
class VulkanDescriptorSetManager;

VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper &layout);

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

    virtual ISwapchain *GetSwapchain() const override;

    virtual IAsyncCompute *GetAsyncCompute() const override
        { return m_async_compute; }

    virtual IFrame *GetCurrentFrame() const override;
    virtual IFrame *PrepareNextFrame() override;
    virtual void PresentFrame(IFrame *frame) override;

    virtual InternalFormat GetDefaultFormat(DefaultImageFormatType type) const override;

    virtual bool IsSupportedFormat(InternalFormat format, ImageSupportType support_type) const override;
    virtual InternalFormat FindSupportedFormat(Span<InternalFormat> possible_formats, ImageSupportType support_type) const override;

    virtual QueryImageCapabilitiesResult QueryImageCapabilities(const TextureDesc &texture_desc) const override;

    virtual Delegate<void, ISwapchain *> &GetOnSwapchainRecreatedDelegate() override
        { return OnSwapchainRecreated; } 

    RendererResult CreateDescriptorSet(const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set);
    RendererResult DestroyDescriptorSet(VkDescriptorSet vk_descriptor_set);
    RC<VulkanDescriptorSetLayoutWrapper> GetOrCreateVkDescriptorSetLayout(const platform::DescriptorSetLayout<Platform::VULKAN> &layout);

private:
    Delegate<void, ISwapchain *>                                OnSwapchainRecreated;

    platform::Instance<Platform::VULKAN>                        *m_instance;

    IRenderConfig                                               *m_render_config;

    CrashHandler                                                m_crash_handler;

    VulkanDescriptorSetManager                                  *m_descriptor_set_manager;

    IAsyncCompute                                               *m_async_compute;

    HashMap<DefaultImageFormatType, InternalFormat>             m_default_formats;

    bool                                                        m_should_recreate_swapchain;
};

} // namespace renderer
} // namespace hyperion

#endif

