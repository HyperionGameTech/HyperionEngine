/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/ParticleVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>

#include <ParticleVolume.generated.inl>

namespace hyperion {

ParticleVolume::ParticleVolume()
    : ParticleVolume(BoundingBox::Empty())
{
}

ParticleVolume::ParticleVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

ParticleVolume::ParticleVolume(const BoundingBox& localBounds, const ParticleVolumeParams& params)
    : VolumeBase(localBounds),
      m_params(params)
{
}

ParticleVolume::~ParticleVolume()
{
    SafeDelete(std::move(m_params.texture));
}

void ParticleVolume::Init()
{
    VolumeBase::Init();

    InitObject(m_params.texture);

    SetReady(true);
}

void ParticleVolume::SetParams(const ParticleVolumeParams& newParams)
{
    m_params = newParams;

    SetNeedsRenderProxyUpdate();
}

void ParticleVolume::UpdateRenderProxy(RenderProxyParticleVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    proxy->particleVolume = WeakHandleFromThis();

    if (proxy->particleTexture != m_params.texture)
    {
        proxy->forceRebind = true;

        proxy->particleTexture = m_params.texture;
    }

    proxy->worldAabb = GetWorldAABB();

    proxy->bufferData.originStartSize = Vec4f(m_params.origin, m_params.startSize);
    proxy->bufferData.spawnRadius = m_params.radius;
    proxy->bufferData.randomness = m_params.randomness;
    proxy->bufferData.avgLifespan = m_params.lifespan;
    proxy->bufferData.maxParticles = uint32(m_params.maxParticles);
    proxy->bufferData.maxParticlesSqrt = MathUtil::Sqrt(float(m_params.maxParticles));
}

} // namespace hyperion
