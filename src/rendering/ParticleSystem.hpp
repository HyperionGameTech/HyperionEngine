/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_PARTICLE_SYSTEM_HPP
#define HYPERION_V2_PARTICLE_SYSTEM_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>
#include <core/ThreadSafeContainer.hpp>
#include <Threads.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <atomic>
#include <mutex>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Frame;
using renderer::Device;
using renderer::Result;

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

class HYP_API ParticleSpawner
    : public BasicObject<STUB_CLASS(ParticleSpawner)>
{
public:
    ParticleSpawner();
    ParticleSpawner(const ParticleSpawnerParams &params);
    ParticleSpawner(const ParticleSpawner &other)               = delete;
    ParticleSpawner &operator=(const ParticleSpawner &other)    = delete;
    ~ParticleSpawner();

    const ParticleSpawnerParams &GetParams() const
        { return m_params; }

    const GPUBufferRef &GetParticleBuffer() const
        { return m_particle_buffer; }

    const GPUBufferRef &GetIndirectBuffer() const
        { return m_indirect_buffer; }

    const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }

    const ComputePipelineRef &GetComputePipeline() const
        { return m_update_particles; }

    BoundingSphere GetBoundingSphere() const
        { return BoundingSphere(m_params.origin, m_params.radius); }

    void Init();
    void Record(CommandBuffer *command_buffer);

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
    Handle<Shader>          m_shader;
    Handle<RenderGroup>     m_render_group;
    Bitmap<1>               m_noise_map;
};

class HYP_API ParticleSystem
    : public BasicObject<STUB_CLASS(ParticleSystem)>
{
public:
    ParticleSystem();
    ParticleSystem(const ParticleSystem &other)             = delete;
    ParticleSystem &operator=(const ParticleSystem &other)  = delete;
    ~ParticleSystem();

    ThreadSafeContainer<ParticleSpawner> &GetParticleSpawners()
        { return m_particle_spawners; }

    const ThreadSafeContainer<ParticleSpawner> &GetParticleSpawners() const
        { return m_particle_spawners; }

    void Init();

    // called in render thread, updates particles using compute shader
    void UpdateParticles(Frame *frame);

    void Render(Frame *frame);

private:
    void CreateBuffers();
    void CreateCommandBuffers();

    Handle<Mesh> m_quad_mesh;

    // for zeroing out data
    GPUBufferRef m_staging_buffer;

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording. size will never change once created
    FixedArray<FixedArray<CommandBufferRef, num_async_rendering_command_buffers>, max_frames_in_flight> m_command_buffers;

    ThreadSafeContainer<ParticleSpawner> m_particle_spawners;

    uint32 m_counter;
};

} // namespace hyperion::v2

#endif