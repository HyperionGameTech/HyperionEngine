/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SAMPLER_HPP
#define HYPERION_BACKEND_RENDERER_SAMPLER_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class SamplerBase : public RenderObject<SamplerBase>
{
public:
    virtual ~SamplerBase() override = default;

    HYP_FORCE_INLINE FilterMode GetMinFilterMode() const
        { return m_min_filter_mode; }
    
    HYP_FORCE_INLINE FilterMode GetMagFilterMode() const
        { return m_mag_filter_mode; }
    
    HYP_FORCE_INLINE WrapMode GetWrapMode() const
        { return m_wrap_mode; }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

protected:
    FilterMode  m_min_filter_mode = FilterMode::TEXTURE_FILTER_NEAREST;
    FilterMode  m_mag_filter_mode = FilterMode::TEXTURE_FILTER_NEAREST;
    WrapMode    m_wrap_mode = WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE;
};

} // namespace renderer
} // namespace hyperion

#endif