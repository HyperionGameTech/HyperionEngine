/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <WebGPUGpuImageView.generated.inl>

namespace Hyperion {

WebGPUGpuImageView::WebGPUGpuImageView(const WebGPUGpuImageRef& image)
    : GpuImageViewBase(image),
      m_viewTextureType(image ? image->GetTextureDesc().type : TextureType::Max)
{
}

WebGPUGpuImageView::WebGPUGpuImageView(
    const WebGPUGpuImageRef& image,
    const ImageSubResource& subResource)
    : GpuImageViewBase(image, subResource),
      m_viewTextureType(image ? image->GetTextureDesc().type : TextureType::Max)
{
}

WebGPUGpuImageView::WebGPUGpuImageView(
    const WebGPUGpuImageRef& image,
    const ImageSubResource& subResource,
    TextureType viewTextureType)
    : GpuImageViewBase(image, subResource),
      m_viewTextureType(viewTextureType == TextureType::Max && image ? image->GetTextureDesc().type : viewTextureType)
{
}

WebGPUGpuImageView::~WebGPUGpuImageView()
{
    ReleaseViews();
}

void WebGPUGpuImageView::ReleaseViews()
{
    Mutex::Guard guard(m_cachedViewsMutex);

    for (CachedView& cachedView : m_cachedViews)
    {
        wgpuTextureViewRelease(cachedView.view);
    }

    m_cachedViews.Clear();
}

bool WebGPUGpuImageView::IsCreated() const
{
    return m_image.IsValid();
}

RendererResult WebGPUGpuImageView::Create()
{
    if (!m_image)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create view for null image!");
    }

    if (!m_image->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Image is not created, cannot create view!");
    }

    return {};
}

WGPUTextureView WebGPUGpuImageView::GetSampledView(WGPUTextureViewDimension dimension)
{
    return GetOrCreateView(ViewUse::Sampled, dimension);
}

WGPUTextureView WebGPUGpuImageView::GetStorageView(WGPUTextureViewDimension dimension)
{
    return GetOrCreateView(ViewUse::Storage, dimension);
}

WGPUTextureView WebGPUGpuImageView::GetAttachmentView()
{
    return GetOrCreateView(ViewUse::Attachment, WGPUTextureViewDimension_2D);
}

WGPUTextureView WebGPUGpuImageView::GetOrCreateView(ViewUse use, WGPUTextureViewDimension dimension)
{
    if (!m_image.IsValid() || !m_image->IsCreated())
    {
        return nullptr;
    }

    WGPUTexture texture = m_image->GetWGPUTexture();

    Mutex::Guard guard(m_cachedViewsMutex);

    for (auto it = m_cachedViews.Begin(); it != m_cachedViews.End();)
    {
        if (it->texture != texture)
        {
            wgpuTextureViewRelease(it->view);
            it = m_cachedViews.Erase(it);

            continue;
        }

        if (it->use == use && it->dimension == dimension)
        {
            return it->view;
        }

        ++it;
    }

    const TextureDesc& textureDesc = m_image->GetTextureDesc();

    const uint32 imageNumMips = textureDesc.HasMipMaps() ? textureDesc.NumMips() : 1;
    const uint32 imageNumLayers = textureDesc.NumArrayLayers();
    const bool isVolume = textureDesc.type == TextureType::Texture3D;

    const uint32 baseMipLevel = MathUtil::Min(uint32(m_subResource.baseMipLevel), imageNumMips - 1);
    const uint32 baseArrayLayer = isVolume ? 0 : MathUtil::Min(uint32(m_subResource.baseArrayLayer), imageNumLayers - 1);

    uint32 mipLevelCount = MathUtil::Min(uint32(m_subResource.numLevels), imageNumMips - baseMipLevel);
    uint32 arrayLayerCount = isVolume ? 1 : MathUtil::Min(uint32(m_subResource.numLayers), imageNumLayers - baseArrayLayer);

    if ((dimension == WGPUTextureViewDimension_3D) != isVolume)
    {
        return nullptr;
    }

    switch (dimension)
    {
    case WGPUTextureViewDimension_2D:
        arrayLayerCount = 1;
        break;
    case WGPUTextureViewDimension_Cube:
        if (imageNumLayers - baseArrayLayer < 6)
        {
            return nullptr;
        }

        arrayLayerCount = 6;
        break;
    case WGPUTextureViewDimension_CubeArray:
        arrayLayerCount -= arrayLayerCount % 6;

        if (arrayLayerCount == 0)
        {
            return nullptr;
        }

        break;
    default:
        break;
    }

    WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    descriptor.dimension = dimension;
    descriptor.baseMipLevel = baseMipLevel;
    descriptor.mipLevelCount = use == ViewUse::Sampled ? mipLevelCount : 1;
    descriptor.baseArrayLayer = baseArrayLayer;
    descriptor.arrayLayerCount = arrayLayerCount;
    descriptor.aspect = use == ViewUse::Sampled ? GetSampledAspect(textureDesc.format) : WGPUTextureAspect_All;

    // a single aspect of a depth-stencil texture has its own format, which WebGPU works out when none is given
    if (descriptor.aspect == WGPUTextureAspect_All)
    {
        descriptor.format = ToWGPUTextureFormat(textureDesc.format);
    }

    WGPUTextureView view = wgpuTextureCreateView(texture, &descriptor);

    if (view == nullptr)
    {
        return nullptr;
    }

    m_cachedViews.PushBack(CachedView { use, dimension, texture, view });

    return view;
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUGpuImageView::SetDebugName(Name name)
{
    GpuImageViewBase::SetDebugName(name);
}
#endif

} // namespace Hyperion
