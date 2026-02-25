/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/math/Mat4f.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/IdGenerator.hpp>

#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct ShadowMapAtlasElement;
class FullScreenPass;

HYP_ENUM()
enum ShadowMapFilter : uint32
{
    SMF_STANDARD = 0,
    SMF_PCF,
    SMF_CONTACT_HARDENED,
    SMF_VSM,

    SMF_MAX
};

HYP_ENUM()
enum ShadowMapType : uint32
{
    SMT_DIRECTIONAL,
    SMT_SPOT,
    SMT_OMNI
};

class ShadowMap
{
public:
    ShadowMap(ShadowMapType type, ShadowMapFilter filterMode, const ShadowMapAtlasElement& atlasElement, const GpuImageViewRef& imageView);
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ~ShadowMap();

    HYP_FORCE_INLINE ShadowMapType GetShadowMapType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE ShadowMapFilter GetFilterMode() const
    {
        return m_filterMode;
    }

    HYP_FORCE_INLINE ShadowMapAtlasElement* GetAtlasElement() const
    {
        return m_atlasElement;
    }

    HYP_FORCE_INLINE const GpuImageViewRef& GetImageView() const
    {
        return m_imageView;
    }

private:
    ShadowMapType m_type;
    ShadowMapFilter m_filterMode;
    ShadowMapAtlasElement* m_atlasElement;
    GpuImageViewRef m_imageView;

    Handle<FullScreenPass> m_combineShadowMapsPass;
};

} // namespace Hyperion
