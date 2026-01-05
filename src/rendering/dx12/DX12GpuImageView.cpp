/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <DX12GpuImageView.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

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

DX12GpuImageView::~DX12GpuImageView()
{
    SafeDelete(std::move(m_image));
}

bool DX12GpuImageView::IsCreated() const
{
    return false;
}

RendererResult DX12GpuImageView::Create()
{
    // @TODO
    HYPERION_RETURN_OK;
}

#ifdef HYP_DEBUG_MODE
void DX12GpuImageView::SetDebugName(Name name)
{
    GpuImageViewBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuImageView

} // namespace Hyperion
