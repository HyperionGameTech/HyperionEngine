/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vector3.hpp>

#include <core/reflection/Handle.hpp>

#include <scene/Volume.hpp>

namespace hyperion {

class Texture;

HYP_STRUCT()
struct ParticleVolumeParams
{
    HYP_STRUCT_BODY(ParticleVolumeParams);

    HYP_FIELD(Serialize = true)
    Handle<Texture> texture;

    HYP_FIELD(Serialize = true)
    uint32 maxParticles = 256u;

    HYP_FIELD(Serialize = true)
    Vec3f origin = Vec3f::Zero();

    HYP_FIELD(Serialize = true)
    float startSize = 0.035f;

    HYP_FIELD(Serialize = true)
    float radius = 1.0f;

    HYP_FIELD(Serialize = true)
    float randomness = 0.5f;

    HYP_FIELD(Serialize = true)
    float lifespan = 1.0f;

    HYP_FIELD(Serialize = true)
    bool hasPhysics = false;
};

HYP_CLASS()
class HYP_API ParticleVolume final : public VolumeBase
{
    HYP_OBJECT_BODY(ParticleVolume);

public:
    ParticleVolume();
    explicit ParticleVolume(const BoundingBox& localBounds);
    ParticleVolume(const BoundingBox& localBounds, const ParticleVolumeParams& params);

    ParticleVolume(const ParticleVolume&) = delete;
    ParticleVolume& operator=(const ParticleVolume&) = delete;

    ~ParticleVolume() override;

    HYP_METHOD(Property = "Params")
    HYP_FORCE_INLINE const ParticleVolumeParams& GetParams() const
    {
        return m_params;
    }

    HYP_METHOD()
    void SetParams(const ParticleVolumeParams& newParams);

    void UpdateRenderProxy(class RenderProxyParticleVolume* proxy);

private:
    void Init() override;

    HYP_FIELD(Serialize = true, Property = "Params")
    ParticleVolumeParams m_params {};
};

} // namespace hyperion
