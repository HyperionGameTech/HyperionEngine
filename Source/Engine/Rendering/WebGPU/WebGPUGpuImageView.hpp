/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuImageView.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Core/Threading/Mutex.hpp>

namespace Hyperion {

class WebGPUGpuImage;

HYP_CLASS(NoScriptBindings)
class WebGPUGpuImageView final : public GpuImageViewBase
{
    HYP_OBJECT_BODY(WebGPUGpuImageView);

public:
    explicit WebGPUGpuImageView(const WebGPUGpuImageRef& image);

    WebGPUGpuImageView(
        const WebGPUGpuImageRef& image,
        const ImageSubResource& subResource);

    WebGPUGpuImageView(
        const WebGPUGpuImageRef& image,
        const ImageSubResource& subResource,
        TextureType viewTextureType);

    ~WebGPUGpuImageView() override;

    HYP_FORCE_INLINE TextureType GetViewTextureType() const
    {
        return m_viewTextureType;
    }

    bool IsCreated() const override;
    RendererResult Create() override;

    // A shader decides the dimension and aspect it reads a texture with, so the WebGPU views are made on demand per use
    WGPUTextureView GetSampledView(WGPUTextureViewDimension dimension);
    WGPUTextureView GetStorageView(WGPUTextureViewDimension dimension);
    WGPUTextureView GetAttachmentView();

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    enum class ViewUse : uint8
    {
        Sampled,
        Storage,
        Attachment
    };

    struct CachedView
    {
        ViewUse use;
        WGPUTextureViewDimension dimension;
        WGPUTexture texture;
        WGPUTextureView view;
    };

    WGPUTextureView GetOrCreateView(ViewUse use, WGPUTextureViewDimension dimension);
    void ReleaseViews();

    TextureType m_viewTextureType = TextureType::Max;

    Array<CachedView, WebGPUAllocator> m_cachedViews;
    Mutex m_cachedViewsMutex;
};

} // namespace Hyperion
