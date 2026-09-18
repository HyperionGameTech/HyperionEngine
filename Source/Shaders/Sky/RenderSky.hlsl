#include "../include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/Scene.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Material.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/EnvProbes.hlsli"
#include "../include/Noise.hlsli"
#include "../include/Entity.hlsli"
#include "../include/Packing.hlsli"

DECLARE_SRV_DYNAMIC(Default, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer entity_instance_batches;
#endif // INSTANCING

DECLARE_SRV_DYNAMIC(Default, CurrentLight) StructuredBuffer<Light> current_light_buffer;
#define light current_light_buffer[0]

DECLARE_SRV(Default, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
    float4x4 vpMatrix;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif // CURRENT_MATERIAL

DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;

#include "../include/Atmosphere.hlsli"

#define NUM_STEPS_X 16
#define NUM_STEPS_Y 16

float3 GetAtmosphere(float3 ray_direction, float3 light_direction, float sun_intensity)
{
    const float3 ray_origin = float3(0.0, 6372e3, 0.0);

    float2 p = RaySphereIntersection(ray_origin, ray_direction, ATMOSPHERE_RADIUS);
    if (p.x > p.y)
    {
        return (float3)0.0;
    }

    p.y = min(p.y, RaySphereIntersection(ray_origin, ray_direction, PLANET_RADIUS).x);

    float2 step_size = float2((p.y - p.x) / float(NUM_STEPS_X), 0.0);

    float3 total_rayleigh = (float3)0.0;
    float3 total_mie = (float3)0.0;

    float2 rayleigh_depths = (float2)0.0;
    float2 mie_depths = (float2)0.0;

    const float mu = dot(ray_direction, light_direction);
    const float mumu = mu * mu;
    const float g = MIE_SCATTER_DIRECTION;
    const float gg = g * g;
    float pRlh = 3.0 / (16.0 * HYP_FMATH_PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * HYP_FMATH_PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    float time = 0.0;

    for (int i = 0; i < NUM_STEPS_X; i++)
    {
        float3 pos_x = ray_origin + ray_direction * (time + step_size.x * 0.5);

        float2 heights = (float2)0.0;

        heights.x = length(pos_x) - PLANET_RADIUS;

        float rayleigh_depth = exp(-heights.x / RAYLEIGH_SCATTER_HEIGHT) * step_size.x;
        float mie_depth = exp(-heights.x / MIE_SCATTER_HEIGHT) * step_size.x;

        rayleigh_depths.x += rayleigh_depth;
        mie_depths.x += mie_depth;

        step_size.y = RaySphereIntersection(pos_x, light_direction, ATMOSPHERE_RADIUS).y / float(NUM_STEPS_Y);

        float ray_time = 0.0;

        rayleigh_depths.y = 0.0;
        mie_depths.y = 0.0;

        for (int j = 0; j < NUM_STEPS_Y; j++)
        {
            float3 pos_y = pos_x + light_direction * (ray_time + step_size.y * 0.5);

            heights.y = length(pos_y) - PLANET_RADIUS;

            rayleigh_depths.y += exp(-heights.y / RAYLEIGH_SCATTER_HEIGHT) * step_size.y;
            mie_depths.y += exp(-heights.y / MIE_SCATTER_HEIGHT) * step_size.y;

            ray_time += step_size.y;
        }

        float3 attenuation = exp(-(MIE_SCATTER_COEFF * (mie_depths.x + mie_depths.y) + RAYLEIGH_SCATTER_COEFF * (rayleigh_depths.x + rayleigh_depths.y)));

        total_rayleigh += rayleigh_depth * attenuation;
        total_mie += mie_depth * attenuation;

        time += step_size.x;
    }

    return sun_intensity * (pRlh * RAYLEIGH_SCATTER_COEFF * total_rayleigh + pMie * MIE_SCATTER_COEFF * total_mie);
}

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : POSITION;
    nointerpolation uint object_index : TEXCOORD1;
};

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

#ifdef INSTANCING
    // We don't use this for sky.
    // Dummy to allow this to compile when precompiling shaders AOT.
    MeshEntityInstanceBatch batch = (MeshEntityInstanceBatch) 0;
#endif // INSTANCING

    float4 position = mul(entity.model_matrix, float4(input.a_position, 1.0));

    output.v_position = position.xyz;

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#else
    output.object_index = 0;
#endif

    output.position_cs = mul(vpMatrix, position);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : POSITION;
    nointerpolation uint object_index : TEXCOORD1;
};

struct PSOutput
{
    float4 output_color : SV_Target0;
};

#define CLEAR_SKY_RADIANCE_PER_SUN_INTENSITY (30.0 / 18.0)
#define OVERCAST_ZENITH_RADIANCE_PER_SUN_INTENSITY 0.08

float3 GetOvercastSky(float3 rayDirection, float3 directionToSun, float sunIntensity)
{
    // dims toward dusk, gone once the sun is well below the horizon
    const float sunElevationFactor = saturate(directionToSun.y * 5.0 + 1.0) * lerp(0.35, 1.0, saturate(directionToSun.y * 2.0));
    const float zenithRadiance = sunIntensity * OVERCAST_ZENITH_RADIANCE_PER_SUN_INTENSITY * world_shader_data.sky_light_params.z * sunElevationFactor;

    // CIE standard overcast sky, three times brighter at the zenith than at the horizon
    float radiance = zenithRadiance * (1.0 + 2.0 * saturate(rayDirection.y)) / 3.0;

    // below the horizon is ground bounce
    radiance *= lerp(0.4, 1.0, saturate(rayDirection.y * 10.0 + 1.0));

    return (float3)radiance;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float3 directionToSun = normalize(light.position_intensity.xyz);
    const float sunIntensity = light.position_intensity.w;
    const float3 rayDirection = normalize(input.v_position);

    const float overcastWeight = smoothstep(0.5, 1.0, world_shader_data.sky_light_params.w);

    float3 skyRadiance = (float3)0.0;

    if (overcastWeight < 1.0)
    {
        skyRadiance = GetAtmosphere(rayDirection, directionToSun, sunIntensity * CLEAR_SKY_RADIANCE_PER_SUN_INTENSITY);
    }

    skyRadiance = lerp(skyRadiance, GetOvercastSky(rayDirection, directionToSun, sunIntensity), overcastWeight);
    skyRadiance *= world_shader_data.sky_tint_intensity.rgb * world_shader_data.sky_tint_intensity.w;

    output.output_color = float4(skyRadiance, 1.0);

    return output;
}

#endif // PIXEL_SHADER
