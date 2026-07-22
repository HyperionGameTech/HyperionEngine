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

#include <Rendering/DX12/DX12DescriptorHeaps.hpp>

namespace Hyperion {

class DX12GpuImage;

HYP_CLASS(NoScriptBindings)
class DX12GpuImageView final : public GpuImageViewBase
{
    HYP_OBJECT_BODY(DX12GpuImageView);

public:
    explicit DX12GpuImageView(const DX12GpuImageRef& image);
    
    DX12GpuImageView(
        const DX12GpuImageRef& image,
        const ImageSubResource& subResource);

    DX12GpuImageView(
        const DX12GpuImageRef& image,
        const ImageSubResource& subResource,
        TextureType viewTextureType);

    ~DX12GpuImageView() override;

    HYP_FORCE_INLINE TextureType GetViewTextureType() const
    {
        return m_viewTextureType;
    }

    bool IsCreated() const override;
    RendererResult Create() override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    TextureType m_viewTextureType = TextureType::Max;
};

} // namespace Hyperion
