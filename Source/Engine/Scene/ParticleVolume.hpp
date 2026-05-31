/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/math/BoundingBox.hpp>
#include <Core/math/Vector3.hpp>

#include <Core/reflection/Handle.hpp>

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

    void UpdateRenderProxy(class RenderProxyParticleVolume* proxy);

    HYP_FIELD(Serialize, Editor)
    Handle<Texture> texture;

    HYP_FIELD(Serialize, Editor)
    Handle<Mesh> mesh;

    HYP_FIELD(Serialize, Editor)
    uint32 maxParticles = 256u;

    HYP_FIELD(Serialize, Editor)
    Vec3f origin = Vec3f::Zero();

    HYP_FIELD(Serialize, Editor)
    float startSize = 0.035f;

    HYP_FIELD(Serialize, Editor)
    float radius = 1.0f;

    HYP_FIELD(Serialize, Editor)
    float randomness = 0.5f;

    HYP_FIELD(Serialize, Editor)
    float lifespan = 1.0f;

    HYP_FIELD(Serialize, Editor)
    bool hasPhysics = false;
};

} // namespace Hyperion
