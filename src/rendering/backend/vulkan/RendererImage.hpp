/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_BACKEND_RENDERER_VULKAN_IMAGE_HPP
#define RENDERER_BACKEND_RENDERER_VULKAN_IMAGE_HPP

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <core/math/MathUtil.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class VulkanImage final : public ImageBase
{
public:
    friend class VulkanSwapchain;

    static constexpr PlatformType platform = Platform::VULKAN;
    
    HYP_API VulkanImage(const TextureDesc &texture_desc);
    HYP_API virtual ~VulkanImage() override;

    HYP_FORCE_INLINE VkImage GetVulkanHandle() const
        { return m_handle; }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Create(ResourceState initial_state) override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API void SetResourceState(ResourceState new_state);

    HYP_API ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;
    HYP_API void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    HYP_API virtual void InsertBarrier(
        CommandBufferBase *command_buffer,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    ) override;

    HYP_API virtual void InsertBarrier(
        CommandBufferBase *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    ) override;

    HYP_API virtual void InsertSubResourceBarrier(
        CommandBufferBase *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    ) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src
    ) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src,
        uint32 src_mip,
        uint32 dst_mip,
        uint32 src_face,
        uint32 dst_face
    ) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect
    ) override;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect,
        uint32 src_mip,
        uint32 dst_mip,
        uint32 src_face,
        uint32 dst_face
    ) override;

    HYP_API virtual RendererResult GenerateMipmaps(CommandBufferBase *command_buffer) override;

    HYP_API virtual void CopyFromBuffer(
        CommandBufferBase *command_buffer,
        const GPUBufferBase *src_buffer
    ) const override;

    HYP_API virtual void CopyToBuffer(
        CommandBufferBase *command_buffer,
        GPUBufferBase *dst_buffer
    ) const override;

    /*! \brief Creates a view of the image for the specified array layer
     */
    HYP_API virtual ImageViewRef MakeLayerImageView(uint32 layer_index) const override;

    HYP_FORCE_INLINE uint8 GetBPP() const
        { return m_bpp; }
    
private:
    VkImage                                     m_handle = VK_NULL_HANDLE;
    VmaAllocation                               m_allocation = VK_NULL_HANDLE;

    VkImageTiling                               m_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags                           m_usage_flags = 0;

    HashMap<uint64, ResourceState>              m_sub_resource_states;

    // true if we created the VkImage, false otherwise (e.g retrieved from swapchain)
    bool                                        m_is_handle_owned = true;

    SizeType                                    m_size;
    SizeType                                    m_bpp; // bytes per pixel
};

} // namespace renderer
} // namespace hyperion

#endif
