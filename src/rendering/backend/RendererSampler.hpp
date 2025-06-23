/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SAMPLER_HPP
#define HYPERION_BACKEND_RENDERER_SAMPLER_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <core/Defines.hpp>

namespace hyperion {
class SamplerBase : public RenderObject<SamplerBase>
{
public:
    virtual ~SamplerBase() override = default;

    HYP_FORCE_INLINE TextureFilterMode GetMinFilterMode() const
    {
        return m_min_filter_mode;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMagFilterMode() const
    {
        return m_mag_filter_mode;
    }

    HYP_FORCE_INLINE TextureWrapMode GetWrapMode() const
    {
        return m_wrap_mode;
    }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

protected:
    TextureFilterMode m_min_filter_mode = TFM_NEAREST;
    TextureFilterMode m_mag_filter_mode = TFM_NEAREST;
    TextureWrapMode m_wrap_mode = TWM_CLAMP_TO_EDGE;
};

} // namespace hyperion

#endif