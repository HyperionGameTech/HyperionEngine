/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/Lightmap.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

Lightmap::Lightmap(Handle<Texture> radiance_texture, Handle<Texture> irradiance_texture)
    : BasicObject(),
      m_radiance_texture(std::move(radiance_texture)),
      m_irradiance_texture(std::move(irradiance_texture))
{
}

Lightmap::~Lightmap() = default;

void Lightmap::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    InitObject(m_radiance_texture);
    InitObject(m_irradiance_texture);

    SetReady(true);
}

} // namespace hyperion::v2
