/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanGpuBuffer.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanGpuImage final : public GpuImageBase
{
    HYP_OBJECT_BODY(VulkanGpuImage);

public:
    friend class VulkanSwapchain;

    explicit VulkanGpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE);
    virtual ~VulkanGpuImage() override;

    HYP_FORCE_INLINE VkImage GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;
    virtual bool IsOwned() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Create(ResourceState initialState) override;

    virtual RendererResult Resize(const Vec3u& extent) override;

    virtual HANDLE GetNativeHandle() const override;

    virtual void SetResourceState(ResourceState newState) override;

    ResourceState GetSubResourceState(const ImageSubResource& subResource) const;
    void SetSubResourceState(const ImageSubResource& subResource, ResourceState newState);

    virtual void InsertBarrier(
        VulkanCommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    virtual void InsertBarrier(
        VulkanCommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    virtual RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* src) override;

    virtual RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* src,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) override;

    virtual RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) override;

    virtual RendererResult Blit(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuImage* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) override;

    virtual RendererResult GenerateMipmaps(VulkanCommandBuffer* commandBuffer) override;

    virtual void CopyFromBuffer(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuBuffer* srcBuffer,
        uint32 srcBufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const override;

    virtual void CopyToBuffer(
        VulkanCommandBuffer* commandBuffer,
        VulkanGpuBuffer* dstBuffer) const override;

    /*! \brief Creates a view of the image for the specified array layer
     */
    virtual VulkanGpuImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    VkImage m_handle = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;

    VkImageTiling m_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags m_usageFlags = 0;

    HashMap<uint64, ResourceState> m_subResourceStates;

    // true if we created the VkImage, false otherwise (e.g retrieved from swapchain)
    bool m_isHandleOwned = true;

    SizeType m_size;
};

} // namespace hyperion
