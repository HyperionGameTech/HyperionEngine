/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12DescriptorHeaps.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <DX12GpuImageView.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12GpuImageView

DX12GpuImageView::DX12GpuImageView(const DX12GpuImageRef& image)
    : GpuImageViewBase(image),
      m_handle {}
{
}

DX12GpuImageView::DX12GpuImageView(
    const DX12GpuImageRef& image,
    uint32 mipIndex,
    uint32 numMips,
    uint32 layerIndex,
    uint32 numLayers)
    : GpuImageViewBase(image, mipIndex, numMips, layerIndex, numLayers),
      m_handle {}
{
}

DX12GpuImageView::~DX12GpuImageView()
{
    SafeDelete(std::move(m_image));
}

bool DX12GpuImageView::IsCreated() const
{
    return m_handle.ptr != 0;
}

RendererResult DX12GpuImageView::Create()
{
    if (!m_image)
        return HYP_MAKE_ERROR(RendererError, "Cannot create view for null image!");

    if (!m_image->IsCreated())
        return HYP_MAKE_ERROR(RendererError, "Image is not created, cannot create view!");

    ID3D12Device* device = g_renderBackend->GetDevice();)

    if (m_image->GetTextureDesc().imageUsage[IU_STORAGE])
    {
        // UAV
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(m_image);

        device->CreateUnorderedAccessView(m_image->GetResource(), nullptr, &uavDesc, m_handle);
    }
    else
    {
        // Create handle
        m_handle = g_renderBackend->descriptorHeapManager->GetDescriptorHeap(DX12DescriptorHeapType::CBV_SRV_UAV, 1);
    }
    // @TODO

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
