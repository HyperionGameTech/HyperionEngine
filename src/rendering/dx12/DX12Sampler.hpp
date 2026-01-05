/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Sampler.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Sampler final : public SamplerBase
{
    HYP_OBJECT_BODY(DX12Sampler);

public:
    DX12Sampler(
        TextureFilterMode minFilterMode = TFM_NEAREST,
        TextureFilterMode magFilterMode = TFM_NEAREST,
        TextureWrapMode wrapMode = TWM_CLAMP_TO_EDGE);

    virtual ~DX12Sampler() override;

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif
};

} // namespace Hyperion
