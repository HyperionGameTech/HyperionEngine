/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <system/vma/VmaUsage.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Types.hpp>

namespace hyperion {
class VulkanGpuImage final : public GpuImageBase
{
public:
    friend class VulkanSwapchain;

    VulkanGpuImage(const TextureDesc& textureDesc);
    virtual ~VulkanGpuImage() override;

    HYP_FORCE_INLINE VkImage GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;
    virtual bool IsOwned() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Create(ResourceState initialState) override;
    virtual RendererResult Destroy() override;

    virtual RendererResult Resize(const Vec3u& extent) override;

    void SetResourceState(ResourceState newState);

    ResourceState GetSubResourceState(const ImageSubResource& subResource) const;
    void SetSubResourceState(const ImageSubResource& subResource, ResourceState newState);

    virtual void InsertBarrier(
        CommandBufferBase* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    virtual void InsertBarrier(
        CommandBufferBase* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src) override;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) override;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) override;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) override;

    virtual RendererResult GenerateMipmaps(CommandBufferBase* commandBuffer) override;

    virtual void CopyFromBuffer(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer) const override;

    virtual void CopyToBuffer(
        CommandBufferBase* commandBuffer,
        GpuBufferBase* dstBuffer) const override;

    /*! \brief Creates a view of the image for the specified array layer
     */
    virtual GpuImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override;
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
