/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <particles/ParticleVolume.hpp>

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

ParticleVolume::ParticleVolume(const BoundingBox& aabb)
    : Entity()
{
    m_entityAabb = aabb;
}

ParticleVolume::ParticleVolume(const BoundingBox& aabb, const ParticleVolumeParams& params)
    : Entity(),
      m_params(params)
{
    m_entityAabb = aabb;
}

ParticleVolume::~ParticleVolume()
{
    SafeDelete(std::move(m_params.texture));
}

void ParticleVolume::Init()
{
    HYP_SCOPE;
    Entity::Init();

    InitObject(m_params.texture);

    SetReady(true);
}

void ParticleVolume::SetParams(const ParticleVolumeParams& params)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    m_params = params;

    SetNeedsRenderProxyUpdate();
}

void ParticleVolume::UpdateRenderProxy(RenderProxyParticleVolume* proxy)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    AssertDebug(proxy != nullptr);

    proxy->particleVolume = WeakHandleFromThis();
    proxy->particleTexture = m_params.texture.Get();
    proxy->worldAabb = GetWorldAABB();

    proxy->bufferData.originStartSize = Vec4f(m_params.origin, m_params.startSize);
    proxy->bufferData.spawnRadius = m_params.radius;
    proxy->bufferData.randomness = m_params.randomness;
    proxy->bufferData.avgLifespan = m_params.lifespan;
    proxy->bufferData.maxParticles = uint32(m_params.maxParticles);
    proxy->bufferData.maxParticlesSqrt = MathUtil::Sqrt(float(m_params.maxParticles));
}

} // namespace hyperion
