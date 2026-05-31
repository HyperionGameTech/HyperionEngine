/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Sampler.hpp>
#endif

#include <Rendering/dx12/DX12Shared.hpp>

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Sampler final : public SamplerBase
{
    HYP_OBJECT_BODY(DX12Sampler);

public:
    explicit DX12Sampler(const SamplerDesc& desc);
    ~DX12Sampler() override;

    HYP_FORCE_INLINE const D3D12_SAMPLER_DESC& GetD3D12SamplerDesc() const
    {
        return m_samplerDesc;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    D3D12_SAMPLER_DESC m_samplerDesc;
    bool m_isCreated = false;
};

} // namespace Hyperion
