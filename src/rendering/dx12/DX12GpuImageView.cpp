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
    if (m_descriptorHandle.IsValid())
        g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_descriptorHandle));

    if (m_image.IsValid())
        SafeDelete(std::move(m_image));
}

bool DX12GpuImageView::IsCreated() const
{
    return m_descriptorHandle.IsValid();
}

RendererResult DX12GpuImageView::Create()
{
    if (!m_image)
        return HYP_MAKE_ERROR(RendererError, "Cannot create view for null image!");

    if (!m_image->IsCreated())
        return HYP_MAKE_ERROR(RendererError, "Image is not created, cannot create view!");

    ID3D12Device* device = g_renderBackend->GetDevice();
    
    // Create handle
    m_descriptorHandle = g_renderBackend->descriptorHeapManager->Allocate(DX12DescriptorHeapType::CBV_SRV_UAV, 1);
    if (!m_descriptorHandle.IsValid())
        return HYP_MAKE_ERROR(RendererError, "Failed to allocate image descriptor handle!");

    if (m_image->GetTextureDesc().imageUsage[IU_STORAGE])
    { // UAV
        const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(m_image);

        device->CreateUnorderedAccessView(m_image->GetResource(), nullptr, &uavDesc, m_descriptorHandle.cpuHandle);
    }
    else
    { // SRV
        const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = GetSRVDesc(m_image);

        device->CreateShaderResourceView(m_image->GetResource(), &srvDesc, m_descriptorHandle.cpuHandle);
    }

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
