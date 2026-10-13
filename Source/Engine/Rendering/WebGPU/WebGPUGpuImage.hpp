/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class WebGPUGpuImage final : public GpuImageBase
{
    HYP_OBJECT_BODY(WebGPUGpuImage);

public:
    explicit WebGPUGpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE);
    ~WebGPUGpuImage() override;

    HYP_FORCE_INLINE WGPUTexture GetWGPUTexture() const
    {
        return m_texture;
    }

    bool IsCreated() const override;
    bool IsOwned() const override;

    RendererResult Create() override;
    RendererResult Create(ResourceState initialState) override;

    RendererResult Resize(const Vec3u& extent) override;

    void InsertBarrier(
        WebGPUCommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType,
        bool onlyDepth = false,
        bool onlyStencil = false) override;

    void InsertBarrier(
        WebGPUCommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType,
        bool onlyDepth = false,
        bool onlyStencil = false) override;

    void Blit(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuImage* srcImage) override;

    void Blit(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuImage* srcImage,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect) override;

    void Blit(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuImage* srcImage,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) override;

    RendererResult GenerateMipmaps(WebGPUCommandBuffer* commandBuffer) override;

    void CopyFromBuffer(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuBuffer* srcBuffer,
        uint32 srcBufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const override;

    void CopyToBuffer(
        WebGPUCommandBuffer* commandBuffer,
        WebGPUGpuBuffer* dstBuffer,
        const ImageSubResource& subResource,
        size_t bufferOffset = 0) const override;

    void CopyFrom(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuImage* srcImage,
        const Vec3u& srcOffset,
        const Vec3u& dstOffset,
        const Vec3u& extent,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) override;

    void Fill(
        WebGPUCommandBuffer* commandBuffer,
        float value,
        const ImageSubResource& subResource,
        const Vec3u& offset = Vec3u::Zero(),
        const Vec3u& extent = Vec3u::One()) override;

    WebGPUGpuImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

    static void ReleaseSharedResources();

private:
    void BlitLevel(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuImage* srcImage,
        uint32 srcMipLevel, uint32 srcArrayLayer,
        uint32 dstMipLevel, uint32 dstArrayLayer,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect);

    // WebGPU only copies whole subresources of depth textures, so a region is drawn instead
    void CopyDepthRegion(
        WebGPUCommandBuffer* commandBuffer,
        const WebGPUGpuImage* srcImage,
        uint32 srcMipLevel, uint32 srcArrayLayer,
        uint32 dstMipLevel, uint32 dstArrayLayer,
        const Vec2u& srcOffset,
        const Vec2u& dstOffset,
        const Vec2u& extent);

    void DestroyTexture();

    WGPUTexture m_texture;
};

} // namespace Hyperion
