/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Constants.hpp>

#include <core/containers/ThreadSafeContainer.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Texture;
struct RenderSetup;

struct ParticleSpawnerParams
{
    Handle<Texture> texture;
    SizeType maxParticles = 256u;
    Vec3f origin = Vec3f::Zero();
    float startSize = 0.035f;
    float radius = 1.0f;
    float randomness = 0.5f;
    float lifespan = 1.0f;
    bool hasPhysics = false;
};

HYP_CLASS()
class HYP_API ParticleSpawner final : public HypObjectBase
{
    HYP_OBJECT_BODY(ParticleSpawner);

public:
    ParticleSpawner();
    ParticleSpawner(const ParticleSpawnerParams& params);
    ParticleSpawner(const ParticleSpawner& other) = delete;
    ParticleSpawner& operator=(const ParticleSpawner& other) = delete;
    ~ParticleSpawner();

    HYP_FORCE_INLINE const ParticleSpawnerParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetParticleBuffer() const
    {
        return m_particleBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetIndirectBuffer() const
    {
        return m_indirectBuffer;
    }

    HYP_FORCE_INLINE const GraphicsPipelineRef& GetGraphicsPipeline() const
    {
        return m_graphicsPipeline;
    }

    HYP_FORCE_INLINE const ComputePipelineRef& GetComputePipeline() const
    {
        return m_updateParticles;
    }

    HYP_FORCE_INLINE BoundingSphere GetBoundingSphere() const
    {
        return BoundingSphere(m_params.origin, m_params.radius);
    }

private:
    void Init() override;

    void CreateNoiseMap();
    void CreateBuffers();
    void CreateComputePipelines();
    void CreateGraphicsPipeline();

    ParticleSpawnerParams m_params;
    GpuBufferRef m_particleBuffer;
    GpuBufferRef m_indirectBuffer;
    GpuBufferRef m_noiseBuffer;
    ComputePipelineRef m_updateParticles;
    GraphicsPipelineRef m_graphicsPipeline;
    ShaderRef m_shader;
    Bitmap<1> m_noiseMap;
};

HYP_CLASS()
class HYP_API ParticleSystem : public HypObjectBase
{
    HYP_OBJECT_BODY(ParticleSystem);

public:
    ParticleSystem();
    ParticleSystem(const ParticleSystem& other) = delete;
    ParticleSystem& operator=(const ParticleSystem& other) = delete;
    ~ParticleSystem();

    HYP_FORCE_INLINE ThreadSafeContainer<ParticleSpawner>& GetParticleSpawners()
    {
        return m_particleSpawners;
    }

    HYP_FORCE_INLINE const ThreadSafeContainer<ParticleSpawner>& GetParticleSpawners() const
    {
        return m_particleSpawners;
    }

    // called in render thread, updates particles using compute shader
    void UpdateParticles(FrameBase* frame, const RenderSetup& renderSetup);

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void Init() override;

    void CreateBuffers();

    Handle<Mesh> m_quadMesh;

    // for zeroing out data
    GpuBufferRef m_stagingBuffer;

    ThreadSafeContainer<ParticleSpawner> m_particleSpawners;

    uint32 m_counter;
};

} // namespace hyperion

