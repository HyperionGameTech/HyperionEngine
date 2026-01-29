/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanGpuBuffer.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanGpuImage final : public GpuImageBase
{
    HYP_OBJECT_BODY(VulkanGpuImage);

public:
    friend class VulkanSwapchain;

    explicit VulkanGpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE);
    ~VulkanGpuImage() override;

    HYP_FORCE_INLINE VkImage GetVulkanHandle() const
    {
        return m_handle;
    }

    bool IsCreated() const override;
    bool IsOwned() const override;

    RendererResult Create() override;
    RendererResult Create(ResourceState initialState) override;

    RendererResult Resize(const Vec3u& extent) override;

    HANDLE GetNativeHandle() const override;

    void SetResourceState(ResourceState newState) override;

    void InsertBarrier(
        VulkanCommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    void InsertBarrier(
        VulkanCommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* srcImage) override;

    RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* srcImage,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) override;
        
    RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* srcImage,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) override;

    RendererResult GenerateMipmaps(VulkanCommandBuffer* commandBuffer) override;

    void CopyFromBuffer(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuBuffer* srcBuffer,
        uint32 srcBufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const override;

    void CopyToBuffer(
        VulkanCommandBuffer* commandBuffer,
        VulkanGpuBuffer* dstBuffer) const override;

    /*! \brief Creates a view of the image for the specified array layer
     */
    VulkanGpuImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    VkImage m_handle = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;

    VkImageTiling m_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags m_usageFlags = 0;

    // true if we created the VkImage, false otherwise (e.g retrieved from swapchain)
    bool m_isHandleOwned = true;

    SizeType m_size;
};

} // namespace Hyperion
