/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/ParticleVolume.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/Threading/Threads.hpp>

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
    //    Check(m_params.texture->Create());
    //}
}

ParticleVolume::~ParticleVolume()
{
    EnqueueDeletion(std::move(texture));
    EnqueueDeletion(std::move(mesh));
}

void ParticleVolume::UpdateRenderProxy(RenderProxyParticleVolume* proxy)
{
    proxy->particleVolume = this;

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


    const Vec3f boxCenter = GetLocalBounds().GetCenter() + origin;
    const Vec3f boxHalfExtent = GetLocalBounds().GetExtent() * 0.5f;

    proxy->bufferData.transformMatrix = GetWorldMatrix() * Mat4f::Translation(boxCenter) * Mat4f::Scaling(boxHalfExtent);
    proxy->bufferData.startSize = startSize;
    proxy->bufferData.randomness = randomness;
    proxy->bufferData.avgLifespan = lifespan;
    proxy->bufferData.maxParticles = maxParticles;
}

} // namespace Hyperion
