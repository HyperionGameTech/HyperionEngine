/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PARTICLE_SYSTEM_HPP
#define HYPERION_PARTICLE_SYSTEM_HPP

#include <Constants.hpp>

#include <core/containers/ThreadSafeContainer.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class Engine;

struct ParticleSpawnerParams
{
    Handle<Texture> texture;
    SizeType        max_particles = 256u;
    Vec3f           origin = Vec3f::Zero();
    float           start_size = 0.035f;
    float           radius = 1.0f;
    float           randomness = 0.5f;
    float           lifespan = 1.0f;
    bool            has_physics = false;
};

HYP_CLASS()
class HYP_API ParticleSpawner : public HypObject<ParticleSpawner>
{
    HYP_OBJECT_BODY(ParticleSpawner);

public:
    ParticleSpawner();
    ParticleSpawner(const ParticleSpawnerParams &params);
    ParticleSpawner(const ParticleSpawner &other)               = delete;
    ParticleSpawner &operator=(const ParticleSpawner &other)    = delete;
    ~ParticleSpawner();

    HYP_FORCE_INLINE const ParticleSpawnerParams &GetParams() const
        { return m_params; }

    HYP_FORCE_INLINE const GPUBufferRef &GetParticleBuffer() const
        { return m_particle_buffer; }

    HYP_FORCE_INLINE const GPUBufferRef &GetIndirectBuffer() const
        { return m_indirect_buffer; }

    HYP_FORCE_INLINE const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }

    HYP_FORCE_INLINE const ComputePipelineRef &GetComputePipeline() const
        { return m_update_particles; }

    HYP_FORCE_INLINE BoundingSphere GetBoundingSphere() const
        { return BoundingSphere(m_params.origin, m_params.radius); }

    void Init();

private:
    void CreateNoiseMap();
    void CreateBuffers();
    void CreateRenderGroup();
    void CreateComputePipelines();

    ParticleSpawnerParams   m_params;
    GPUBufferRef            m_particle_buffer;
    GPUBufferRef            m_indirect_buffer;
    GPUBufferRef            m_noise_buffer;
    ComputePipelineRef      m_update_particles;
    ShaderRef               m_shader;
    Handle<RenderGroup>     m_render_group;
    Bitmap<1>               m_noise_map;
};

HYP_CLASS()
class HYP_API ParticleSystem : public HypObject<ParticleSystem>
{
    HYP_OBJECT_BODY(ParticleSystem);

public:
    ParticleSystem();
    ParticleSystem(const ParticleSystem &other)             = delete;
    ParticleSystem &operator=(const ParticleSystem &other)  = delete;
    ~ParticleSystem();

    HYP_FORCE_INLINE ThreadSafeContainer<ParticleSpawner> &GetParticleSpawners()
        { return m_particle_spawners; }

    HYP_FORCE_INLINE const ThreadSafeContainer<ParticleSpawner> &GetParticleSpawners() const
        { return m_particle_spawners; }

    void Init();

    // called in render thread, updates particles using compute shader
    void UpdateParticles(FrameBase *frame);

    void Render(FrameBase *frame);

private:
    void CreateBuffers();

    Handle<Mesh>                                                                                        m_quad_mesh;

    // for zeroing out data
    GPUBufferRef                                                                                        m_staging_buffer;

    ThreadSafeContainer<ParticleSpawner>                                                                m_particle_spawners;

    uint32                                                                                              m_counter;
};

} // namespace hyperion

#endif