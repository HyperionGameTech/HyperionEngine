/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuImageView.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

class DX12GpuImage;

HYP_CLASS(NoScriptBindings)
class DX12GpuImageView final : public GpuImageViewBase
{
    HYP_OBJECT_BODY(DX12GpuImageView);

public:
    DX12GpuImageView(const DX12GpuImageRef& image);
    DX12GpuImageView(
        const DX12GpuImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 layerIndex,
        uint32 numLayers);

    ~DX12GpuImageView() override;

    HYP_FORCE_INLINE const D3D12_CPU_DESCRIPTOR_HANDLE& GetCpuDescriptorHandle() const
    {
        return m_handle;
    }

    bool IsCreated() const override;
    RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_handle;
};

} // namespace Hyperion
