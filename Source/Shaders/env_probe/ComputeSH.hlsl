#include "../include/defines.inc"

#define NUM_SAMPLES_X 16
#define NUM_SAMPLES_Y 16

#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

struct SHTile
{
    float4 coeffs_weights[9];
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(ComputeSH, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(ComputeSH, SamplerNearest) SamplerState sampler_nearest;

#include "../include/shared.inc"
#include "../include/packing.inc"
#include "../include/scene.inc"
#include "../include/Octahedron.inc"
#include "../include/env_probe.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(ComputeSH, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(ComputeSH, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

#if defined(MODE_CLEAR) || defined(MODE_FINALIZE)
DECLARE_UAV(ComputeSH, EnvProbesBuffer) RWStructuredBuffer<EnvProbe> env_probes;
#endif

#ifdef LIGHTING
DECLARE_SRV_DYNAMIC(ComputeSH, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

DECLARE_SRV(ComputeSH, ShadowMapsTextureArray) Texture2DArray shadow_maps;
DECLARE_SRV(ComputeSH, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

DECLARE_SRV_DYNAMIC(ComputeSH, CurrentLight) StructuredBuffer<Light> current_light_buffer;
#define currentLight current_light_buffer[0]

#include "../include/Shadows.hlsli"
#endif

DECLARE_SRV(ComputeSH, InColorCubemap) TextureCube cubemap_color;

#if defined(MODE_BUILD_COEFFICIENTS) && defined(LIGHTING)
DECLARE_SRV(ComputeSH, InNormalsCubemap) TextureCube cubemap_normals;
DECLARE_SRV(ComputeSH, InDepthCubemap) TextureCube cubemap_depth;
#endif

#if defined(MODE_BUILD_COEFFICIENTS) || defined(MODE_FINALIZE) || defined(MODE_CLEAR)
DECLARE_UAV(ComputeSH, InputSHTilesBuffer) RWStructuredBuffer<SHTile> sh_tiles;
#endif

#ifdef MODE_REDUCE
DECLARE_UAV(ComputeSH, OutputSHTilesBuffer) RWStructuredBuffer<SHTile> sh_tiles_output;
#endif

#if defined(MODE_FINALIZE) || defined(MODE_REDUCE) || defined(MODE_CLEAR) || (defined(MODE_BUILD_COEFFICIENTS) && defined(LIGHTING))
DECLARE_BUFFER(ComputeSH, SHUniforms) cbuffer SHUniforms
{
    uint4 probe_grid_position;
    uint4 cubemap_dimensions;
    uint4 level_dimensions;
    float4 world_position;
    uint env_probe_index;
};
#endif

#if defined(MODE_BUILD_COEFFICIENTS) || defined(MODE_CLEAR)
#define CURRENT_TILE sh_tiles[(sample_index.x * NUM_SAMPLES_Y) + sample_index.y]
#endif

void ProjectOntoSHBands(float3 dir, out float sh[9])
{
    sh[0] = 0.282095f;

    sh[1] = 0.488603f * dir.y;
    sh[2] = 0.488603f * dir.z;
    sh[3] = 0.488603f * dir.x;

    sh[4] = 1.092548f * dir.x * dir.y;
    sh[5] = 1.092548f * dir.y * dir.z;
    sh[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
    sh[7] = 1.092548f * dir.x * dir.z;
    sh[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);
}

void ProjectOntoSH9Color(float3 dir, float3 color, out float sh_colors[27])
{
    float bands[9];
    ProjectOntoSHBands(dir, bands);

    for (uint i = 0; i < 9; ++i)
    {
        sh_colors[i * 3] = color.r * bands[i];
        sh_colors[i * 3 + 1] = color.g * bands[i];
        sh_colors[i * 3 + 2] = color.b * bands[i];
    }
}

#ifdef LIGHTING
float4 SampleSky(float3 dir)
{
    if (current_env_probe.texture_index != ~0u)
    {
        uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

        return EnvProbeSample(sampler_linear, envProbesTexture, probe_texture_index, dir, 0.0);
    }

    return float4(0.0, 0.0, 0.0, 0.0);
}

float4 CalculateDirectLighting(in float3 P, in float3 N)
{
    if (currentLight.type != HYP_LIGHT_TYPE_DIRECTIONAL)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    const float4 light_color = currentLight.color;

    float3 L = normalize(currentLight.position_intensity.xyz);

    float NdotL = max(0.0001, dot(N, L));

    float shadow = 1.0;

    return float4(light_color.rgb * NdotL * shadow * currentLight.position_intensity.w, 1.0);
}
#endif

#ifdef MODE_REDUCE
groupshared float4 shared_memory[9][6];
#endif

#if defined(MODE_BUILD_COEFFICIENTS) || defined(MODE_CLEAR)
[numthreads(NUM_SAMPLES_X, NUM_SAMPLES_Y, 1)]
#elif defined(MODE_REDUCE)
[numthreads(6, 4, 4)]
#elif defined(MODE_FINALIZE)
[numthreads(1, 1, 1)]
#endif
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
#ifdef MODE_CLEAR
    const uint2 sample_index = uint2(dispatchThreadID.xy);

    for (int i = 0; i < 9; i++)
    {
        CURRENT_TILE.coeffs_weights[i] = float4(0.0, 0.0, 0.0, 0.0);
    }

    if (all(dispatchThreadID.xyz == uint3(0, 0, 0)))
    {
        for (int i = 0; i < 9; i++)
        {
            env_probes[env_probe_index].sh[i] = float4(0.0, 0.0, 0.0, 0.0);
        }
    }
#elif defined(MODE_BUILD_COEFFICIENTS)
    const uint2 sample_index = uint2(dispatchThreadID.xy);

    const float2 uv = (float2(sample_index) + 0.5) / float2(NUM_SAMPLES_X, NUM_SAMPLES_Y);
    const float2 sample_point = uv * 2.0 - 1.0;
    const float3 dir = normalize(DecodeOctahedralCoord(uv));

    float4 albedo = SAMPLE_TEXTURE_CUBE(sampler_linear, cubemap_color, dir);

#ifdef LIGHTING
    float3 normal = normalize(UnpackNormalVec2(SAMPLE_TEXTURE_CUBE(sampler_nearest, cubemap_normals, dir).rg));
    float2 dist_dist2 = SAMPLE_TEXTURE_CUBE(sampler_nearest, cubemap_depth, dir).rg;

    float4 position = float4(world_position.xyz + dir * dist_dist2.r, 1.0);

    float4 indirect = SampleSky(normal);
    float4 color = (indirect + CalculateDirectLighting(position.xyz, normal)) * (albedo * (1.0 / HYP_FMATH_PI));
    color.a = 1.0;
#else
    float4 color = albedo;
#endif

    float sh_values[27];
    ProjectOntoSH9Color(dir, color.rgb, sh_values);

    float temp = 1.0 + sample_point.x * sample_point.x + sample_point.y * sample_point.y;
    float weight = 4.0 / (sqrt(temp) * temp);

    for (int i = 0; i < 9; i++)
    {
        float3 sh_color = float3(
            sh_values[i * 3],
            sh_values[i * 3 + 1],
            sh_values[i * 3 + 2]);

        CURRENT_TILE.coeffs_weights[i] = float4(sh_color * weight, weight);
    }
#elif defined(MODE_REDUCE)
    const int face_index = int(dispatchThreadID.x);

    if (face_index >= 6)
    {
        return;
    }

    const int2 input_index = int2(dispatchThreadID.yz * 2);
    const int2 output_index = int2(dispatchThreadID.yz);

    const int2 prev_dimensions = int2(level_dimensions.xy);
    const int2 next_dimensions = int2(level_dimensions.zw);

    if (any(input_index >= prev_dimensions))
    {
        return;
    }

    if (any(input_index + 1 >= prev_dimensions))
    {
        return;
    }

    if (any(output_index >= next_dimensions))
    {
        return;
    }

    for (int i = 0; i < 9; i++)
    {
        sh_tiles_output[(face_index * next_dimensions.x * next_dimensions.y) + (output_index.x * next_dimensions.y) + output_index.y].coeffs_weights[i] =
            sh_tiles[(face_index * prev_dimensions.x * prev_dimensions.y) + (input_index.x * prev_dimensions.y) + input_index.y].coeffs_weights[i]
            + sh_tiles[(face_index * prev_dimensions.x * prev_dimensions.y) + ((input_index.x + 1) * prev_dimensions.y) + input_index.y].coeffs_weights[i]
            + sh_tiles[(face_index * prev_dimensions.x * prev_dimensions.y) + ((input_index.x + 1) * prev_dimensions.y) + (input_index.y + 1)].coeffs_weights[i]
            + sh_tiles[(face_index * prev_dimensions.x * prev_dimensions.y) + (input_index.x * prev_dimensions.y) + (input_index.y + 1)].coeffs_weights[i];
    }
#elif defined(MODE_FINALIZE)

#ifdef PARALLEL_REDUCE
    for (int face_index = 0; face_index < 6; face_index++)
    {
        for (int i = 0; i < 9; i++)
        {
            env_probes[env_probe_index].sh[i] += sh_tiles[face_index * 9 + i].coeffs_weights[i];
        }
    }

    for (int i = 0; i < 9; i++)
    {
        float weight = env_probes[env_probe_index].sh[i].a;

        env_probes[env_probe_index].sh[i] *= (4.0 * HYP_FMATH_PI) / max(weight, 0.0001);
    }
#else
    float total_weight = 0.0;

    float3 sh_result[9];

    for (int i = 0; i < 9; i++)
    {
        sh_result[i] = float3(0.0, 0.0, 0.0);
    }

    for (int sample_x = 0; sample_x < NUM_SAMPLES_X; ++sample_x)
    {
        for (int sample_y = 0; sample_y < NUM_SAMPLES_Y; ++sample_y)
        {
            for (int i = 0; i < 9; i++)
            {
                const float3 coeff = sh_tiles[(sample_x * NUM_SAMPLES_Y) + sample_y].coeffs_weights[i].rgb;
                const float weight = sh_tiles[(sample_x * NUM_SAMPLES_Y) + sample_y].coeffs_weights[i].a;

                sh_result[i] += coeff;

                total_weight += weight;
            }
        }
    }

    for (int i = 0; i < 9; i++)
    {
        float3 result = float3(sh_result[i] *= (4.0 * HYP_FMATH_PI) / total_weight);

        env_probes[env_probe_index].sh[i] = float4(result, 1.0);
    }
#endif

#endif
}
