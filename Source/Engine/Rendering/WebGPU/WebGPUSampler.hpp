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

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class WebGPUSampler final : public SamplerBase
{
    HYP_OBJECT_BODY(WebGPUSampler);

public:
    explicit WebGPUSampler(const SamplerDesc& desc);
    ~WebGPUSampler() override;

    HYP_FORCE_INLINE WGPUSampler GetWGPUSampler() const
    {
        return m_sampler;
    }

    HYP_FORCE_INLINE bool IsFiltering() const
    {
        return m_isFiltering;
    }

    HYP_FORCE_INLINE bool IsComparison() const
    {
        return m_compareOp != SamplerCompareOp::None;
    }

    bool IsCreated() const override;
    RendererResult Create() override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    WGPUSampler m_sampler;
    bool m_isFiltering;
};

} // namespace Hyperion
