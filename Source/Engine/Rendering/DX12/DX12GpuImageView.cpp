/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12GpuImageView.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>
#include <Rendering/DX12/DX12DescriptorHeaps.hpp>
#include <Rendering/DX12/DX12Helpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <DX12GpuImageView.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12GpuImageView

DX12GpuImageView::DX12GpuImageView(const DX12GpuImageRef& image)
    : GpuImageViewBase(image),
      m_viewTextureType(image ? image->GetTextureDesc().type : TextureType::Max)
{
}

DX12GpuImageView::DX12GpuImageView(
    const DX12GpuImageRef& image,
    const ImageSubResource& subResource)
    : GpuImageViewBase(image, subResource),
      m_viewTextureType(image ? image->GetTextureDesc().type : TextureType::Max)
{
}

DX12GpuImageView::DX12GpuImageView(
    const DX12GpuImageRef& image,
    const ImageSubResource& subResource,
    TextureType viewTextureType)
    : GpuImageViewBase(image, subResource),
      m_viewTextureType(viewTextureType)
{
}

DX12GpuImageView::~DX12GpuImageView() = default;

bool DX12GpuImageView::IsCreated() const
{
    return m_image != nullptr;
}

RendererResult DX12GpuImageView::Create()
{
    if (!m_image)
        return HYP_MAKE_ERROR(RendererError, "Cannot create view for null image!");

    if (!m_image->IsCreated())
        return HYP_MAKE_ERROR(RendererError, "Image is not created, cannot create view!");

    return {};
}

#ifdef HYP_RHI_DEBUG_NAMES
void DX12GpuImageView::SetDebugName(Name name)
{
    GpuImageViewBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuImageView

} // namespace Hyperion
