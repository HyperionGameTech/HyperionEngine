/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_HPP
#define HYPERION_LIGHTMAP_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <rendering/Texture.hpp>

namespace hyperion {

class HYP_API Lightmap
    : public BasicObject<STUB_CLASS(Lightmap)>
{
public:
    Lightmap(Handle<Texture> radiance_texture, Handle<Texture> irradiance_texture);
    Lightmap(const Lightmap &other)                 = delete;
    Lightmap &operator=(const Lightmap &other)      = delete;
    Lightmap(Lightmap &&other) noexcept             = delete;
    Lightmap &operator=(Lightmap &&other) noexcept  = delete;
    ~Lightmap();

    const Handle<Texture> &GetRadianceTexture() const
        { return m_radiance_texture; }

    const Handle<Texture> &GetIrradianceTexture() const
        { return m_irradiance_texture; }
    
    void Init();

protected:
    Handle<Texture> m_radiance_texture;
    Handle<Texture> m_irradiance_texture;
};

} // namespace hyperion

#endif