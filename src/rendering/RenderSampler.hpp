/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <rendering/RenderObject.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class SamplerBase : public HypObjectBase
{
    HYP_OBJECT_BODY(SamplerBase);

public:
    virtual ~SamplerBase() override = default;

    HYP_FORCE_INLINE TextureFilterMode GetMinFilterMode() const
    {
        return m_minFilterMode;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMagFilterMode() const
    {
        return m_magFilterMode;
    }

    HYP_FORCE_INLINE TextureWrapMode GetWrapMode() const
    {
        return m_wrapMode;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

protected:
    TextureFilterMode m_minFilterMode = TFM_NEAREST;
    TextureFilterMode m_magFilterMode = TFM_NEAREST;
    TextureWrapMode m_wrapMode = TWM_CLAMP_TO_EDGE;

    Name m_debugName;
};

} // namespace hyperion
