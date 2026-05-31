/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/ParticleVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/MeshBuilder.hpp>

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
    //if (!m_params.mesh.IsValid())
    //{
    //    m_params.mesh = MeshBuilder::Quad();
    //}
    //
    //m_params.mesh->SetFlags(MeshFlags::ViewIndependent);
    //m_params.mesh->UploadGpuData();

    //if (m_params.texture.IsValid())
    //{
    //    CheckResult(m_params.texture->Create());
    //}
}

ParticleVolume::~ParticleVolume()
{
    EnqueueDeletion(std::move(texture));
    EnqueueDeletion(std::move(mesh));
}

void ParticleVolume::UpdateRenderProxy(RenderProxyParticleVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    proxy->particleVolume = WeakHandleFromThis();

    if (proxy->particleTexture != texture)
    {
        proxy->forceRebind = true;

        proxy->particleTexture = texture;
    }

    if (proxy->particleMesh != mesh)
    {
        proxy->forceRebind = true;

        proxy->particleMesh = mesh;
    }

    proxy->worldAabb = GetWorldBounds();

    proxy->bufferData.originStartSize = Vec4f(origin, startSize);
    proxy->bufferData.spawnRadius = radius;
    proxy->bufferData.randomness = randomness;
    proxy->bufferData.avgLifespan = lifespan;
    proxy->bufferData.maxParticles = uint32(maxParticles);
    proxy->bufferData.maxParticlesSqrt = MathUtil::Sqrt(float(maxParticles));
}

} // namespace Hyperion
