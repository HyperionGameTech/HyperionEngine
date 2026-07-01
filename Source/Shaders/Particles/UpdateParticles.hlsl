#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"

DECLARE_SAMPLER(UpdateParticlesDescriptorSet, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(UpdateParticlesDescriptorSet, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(UpdateParticlesDescriptorSet, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;
DECLARE_SRV(UpdateParticlesDescriptorSet, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(UpdateParticlesDescriptorSet, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(UpdateParticlesDescriptorSet, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;
DECLARE_SRV(UpdateParticlesDescriptorSet, GBufferDepthTexture) Texture2D GBufferDepthTexture;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#include "../include/Entity.hlsli"
#include "../include/Packing.hlsli"
#include "../include/Gbuffer.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

struct ParticleShaderData
{
    float4 position;   // w = scale
    float4 velocity;   // w = final scale
    float4 color;
    float4 attributes; // x = lifetime
};

#ifndef MAX_PARTICLES
#define MAX_PARTICLES 1024
#endif

DECLARE_UAV(UpdateParticlesDescriptorSet, ParticlesBuffer) RWStructuredBuffer<ParticleShaderData> instances;

#define HYP_PARTICLE_NOISE_MAP_EXTENT 128
#define HYP_PARTICLE_NOISE_MAP_SIZE HYP_FMATH_SQR(HYP_PARTICLE_NOISE_MAP_EXTENT)

struct IndirectDrawCommand
{
    // VkDrawIndexedIndirectCommand
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

DECLARE_UAV(UpdateParticlesDescriptorSet, IndirectDrawCommandsBuffer) RWStructuredBuffer<IndirectDrawCommand> indirect_draw_command;

DECLARE_SRV(UpdateParticlesDescriptorSet, NoiseMap) Texture2D noise_map;

DECLARE_SRV(UpdateParticlesDescriptorSet, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_BUFFER_DYNAMIC(UpdateParticlesDescriptorSet, CBuffer) cbuffer CBuffer
{
    float4 origin;

    float spawn_radius;
    float randomness;
    float avg_lifespan;
    uint max_particles;

    float max_particles_sqrt;
    float delta_time;
    uint global_counter;
    uint _pad;

    Camera camera;
};

float3 GetNoiseValue(uint id)
{
    const uint baseOffset = id * 3u;

    const uint3 indices = uint3(
        (global_counter + baseOffset) % HYP_PARTICLE_NOISE_MAP_SIZE,
        (global_counter + baseOffset + 1u) % HYP_PARTICLE_NOISE_MAP_SIZE,
        (global_counter + baseOffset + 2u) % HYP_PARTICLE_NOISE_MAP_SIZE);

    float2 uv0 = (float2)(float(indices.x % HYP_PARTICLE_NOISE_MAP_EXTENT) + 0.5) / float(HYP_PARTICLE_NOISE_MAP_EXTENT);
    float2 uv1 = (float2)(float(indices.y % HYP_PARTICLE_NOISE_MAP_EXTENT) + 0.5) / float(HYP_PARTICLE_NOISE_MAP_EXTENT);
    float2 uv2 = (float2)(float(indices.z % HYP_PARTICLE_NOISE_MAP_EXTENT) + 0.5) / float(HYP_PARTICLE_NOISE_MAP_EXTENT);

    float3 noiseValue = float3(
        SAMPLE_TEXTURE_2D(sampler_linear, noise_map, uv0).r * 2.0 - 1.0,
        SAMPLE_TEXTURE_2D(sampler_linear, noise_map, uv1).r * 2.0 - 1.0,
        SAMPLE_TEXTURE_2D(sampler_linear, noise_map, uv2).r * 2.0 - 1.0);

    return noiseValue;
}

static const float s_maxParticlesSqrt = sqrt(max(float(MAX_PARTICLES), 1.0));
static const bool s_fadeAlpha = false;

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint id = dispatchThreadID.x;
    const uint index = id;

    if (id >= MAX_PARTICLES)
    {
        return;
    }

    float time = float(global_counter) * delta_time;

    ParticleShaderData particle = instances[index];

    float3 nextPos = particle.position.xyz + (particle.velocity.xyz * delta_time);

    float has_gravity = 1.0;
    const float avg_lifespan_value = max(avg_lifespan, 0.01);

    float3 noiseValue = GetNoiseValue(id);
    float3 randDir = normalize(noiseValue);
    float3 startingVelocity = randDir;

    float3 startingPos = origin.xyz;

    float2 posIdx = float2(fmod(float(id), s_maxParticlesSqrt), float(id) / s_maxParticlesSqrt);

    startingPos += float3(sin(posIdx.x / float(MAX_PARTICLES) * HYP_FMATH_PI), 0.0, cos(posIdx.y / float(MAX_PARTICLES) * HYP_FMATH_PI))
        * randomness
        * (noiseValue.y * 0.5 + 0.5)
        * spawn_radius;

    float startingLifetime = avg_lifespan_value;
    startingLifetime += noiseValue.z * randomness * (avg_lifespan_value * 0.5);

    float lifetime = particle.attributes.x;

    const float lifetime_ratio = clamp(lifetime / avg_lifespan_value, 0.0, 1.0);

    const float currentScale = particle.position.w;
    const float finalScale = particle.velocity.w;
    const float startingScale = origin.w;
    const float startingFinalScale = startingScale * ((noiseValue.y + 1.0) * randomness);
    const float nextScale = lerp(currentScale, finalScale, lifetime_ratio);

    // reset the particle if lifetime has expired
    float isAlive = step(0.0, lifetime);
    particle.position.xyz = lerp(startingPos, nextPos, isAlive);
    particle.position.w = lerp(startingScale, nextScale, isAlive);
    particle.velocity.xyz = lerp(startingVelocity, particle.velocity.xyz - (float3(0.0, 9.81, 0.0) * delta_time * has_gravity), isAlive);
    particle.velocity.w = lerp(startingFinalScale, finalScale, isAlive);

    lifetime = lerp(startingLifetime, lifetime - delta_time, isAlive);
    particle.attributes.x = lifetime;

#ifdef HAS_PHYSICS
    float4 posNDC = mul(camera.viewProjMat, float4(particle.position.xyz, 1.0));
    posNDC /= posNDC.w;

    const float2 particleUV = float2(posNDC.x * 0.5 + 0.5, 1.0 - (posNDC.y * 0.5 + 0.5));

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferDepthTexture, particleUV).r;

    float4 posWS = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, particleUV, depth);
    posWS /= posWS.w;

    if (distance(posWS.xyz, particle.position.xyz) <= 1.0)
    {
        const float3 normal = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, GBufferNormalsTexture, particleUV));

        particle.velocity.xyz = reflect(normal, normalize(particle.velocity.xyz));
    }
#endif

    instances[index] = particle;

    uint original_value;
    InterlockedAdd(indirect_draw_command[0].instance_count, 1u, original_value);
}
