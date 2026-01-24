/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12DescriptorHeaps.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <DX12GpuImageView.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12GpuImageView

DX12GpuImageView::DX12GpuImageView(const DX12GpuImageRef& image)
    : GpuImageViewBase(image)
{
}

DX12GpuImageView::DX12GpuImageView(
    const DX12GpuImageRef& image,
    uint32 mipIndex,
    uint32 numMips,
    uint32 layerIndex,
    uint32 numLayers)
    : GpuImageViewBase(image, mipIndex, numMips, layerIndex, numLayers)
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

#ifdef HYP_DEBUG_MODE
void DX12GpuImageView::SetDebugName(Name name)
{
    GpuImageViewBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuImageView

} // namespace Hyperion
