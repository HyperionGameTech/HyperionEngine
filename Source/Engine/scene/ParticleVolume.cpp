/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/ParticleVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/threading/Threads.hpp>

#include <ParticleVolume.generated.inl>

namespace Hyperion {

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
    EnqueueDeletion(std::move(m_params.texture));
}

void ParticleVolume::Init()
{
    VolumeBase::Init();

    if (m_params.texture.IsValid())
    {
        CheckResult(m_params.texture->Create());
    }

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

    proxy->worldAabb = GetWorldBounds();

    proxy->bufferData.originStartSize = Vec4f(m_params.origin, m_params.startSize);
    proxy->bufferData.spawnRadius = m_params.radius;
    proxy->bufferData.randomness = m_params.randomness;
    proxy->bufferData.avgLifespan = m_params.lifespan;
    proxy->bufferData.maxParticles = uint32(m_params.maxParticles);
    proxy->bufferData.maxParticlesSqrt = MathUtil::Sqrt(float(m_params.maxParticles));
}

} // namespace Hyperion
