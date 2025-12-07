/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/FogVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Shared.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/MathUtil.hpp>

#include <engine/EngineGlobals.hpp>

#include <FogVolume.generated.inl>

namespace hyperion {

static constexpr uint32 MaxVolumeTextureExtent = 32;

FogVolume::FogVolume()
    : FogVolume(BoundingBox::Empty())
{
}

FogVolume::FogVolume(const BoundingBox& aabb)
    : Entity()
{
    m_localBounds = aabb;
}

FogVolume::~FogVolume()
{
    SafeDelete(std::move(m_volumeTexture));
}

void FogVolume::Init()
{
    Entity::Init();

    if (!m_volumeTexture)
    {
        Vec3u volumeTextureDimensions;
        const Vec3f localBoundsExtent = m_localBounds.GetExtent();

        const float maxExtent = localBoundsExtent.Max();

        if (maxExtent < MathUtil::epsilonF)
        {
            volumeTextureDimensions = Vec3u::One();
        }
        else
        {
            const float scale = float(MaxVolumeTextureExtent) / maxExtent;
            volumeTextureDimensions = Vec3u(MathUtil::Max(Vec3f(1.0f), MathUtil::Ceil(localBoundsExtent * scale)));
        }

        if (volumeTextureDimensions.Volume() == 0)
        {
            volumeTextureDimensions = Vec3u::One();
        }

        TextureDesc desc {};
        desc.type = TT_TEX3D;
        desc.format = TF_RGBA8;
        desc.extent = volumeTextureDimensions;

        m_volumeTexture = CreateObject<Texture>(desc);
    }

    InitObject(m_volumeTexture);

    SetReady(true);
}

} // namespace hyperion
