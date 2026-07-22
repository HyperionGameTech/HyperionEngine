/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Scene/Volume.hpp>

namespace Hyperion {

class Texture;
class Mesh;

HYP_CLASS()
class ENGINE_API ParticleVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(ParticleVolume);

public:
    ParticleVolume();
    explicit ParticleVolume(const BoundingBox& localBounds);

    ParticleVolume(const ParticleVolume&) = delete;
    ParticleVolume& operator=(const ParticleVolume&) = delete;

    ~ParticleVolume() override;

    void UpdateRenderProxy(struct RenderProxyParticleVolume* proxy);

    HYP_FIELD(Property = "ParticleTexture", Serialize, Editor, Title = "Particle Texture")
    Handle<Texture> texture;

    HYP_FIELD(Property = "ParticleMesh", Serialize, Editor, Title = "Particle Mesh")
    Handle<Mesh> mesh;

    HYP_FIELD(Property = "MaxParticles", Serialize, Editor, Title = "Max Particles Active")
    uint32 maxParticles = 256u;

    HYP_FIELD(Property = "Origin", Serialize, Editor, Title = "Origin Position")
    Vec3f origin = Vec3f::Zero();

    HYP_FIELD(Property = "StartSize", Serialize, Editor, Title = "Particle Size")
    float startSize = 0.035f;

    HYP_FIELD(Property = "Randomness", Serialize, Editor, Title = "Randomness")
    float randomness = 0.5f;

    HYP_FIELD(Property = "Lifespan", Serialize, Editor, Title = "Particle Lifespan")
    float lifespan = 1.0f;

    HYP_FIELD(Property = "EnableCollision", Serialize, Editor, Title = "Enable Collision")
    bool enableCollision = false;
};

} // namespace Hyperion
