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

#if defined(MODE_CLEAR) || defined(MODE_FINALIZE)
// @TODO Instead of huge RWStructuredBuffer for env probes we should use a one-off
// sbuffer to write/read from since we end up reading from it onto the cpu anyway.
// And eventually we will get rid of some of these huge buffers because of the new constants allocator
// that lets us treat everything as push constants essentially.
DECLARE_UAV(ComputeSH, EnvProbesBuffer) RWStructuredBuffer<EnvProbe> env_probes;
#endif

DECLARE_SRV(ComputeSH, InColorCubemap) TextureCube cubemap_color;

#if defined(MODE_BUILD_COEFFICIENTS) || defined(MODE_FINALIZE) || defined(MODE_CLEAR)
DECLARE_UAV(ComputeSH, InputSHTilesBuffer) RWStructuredBuffer<SHTile> sh_tiles;
#endif

#ifdef MODE_REDUCE
DECLARE_UAV(ComputeSH, OutputSHTilesBuffer) RWStructuredBuffer<SHTile> sh_tiles_output;
#endif

#if defined(MODE_FINALIZE) || defined(MODE_REDUCE) || defined(MODE_CLEAR) || (defined(MODE_BUILD_COEFFICIENTS))
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
        CURRENT_TILE.coeffs_weights[i] = (float4)0.0;
    }

    if (all(dispatchThreadID.xyz == uint3(0, 0, 0)))
    {
        for (int i = 0; i < 9; i++)
        {
            env_probes[env_probe_index].sh[i] = (float4)0.0;
        }
    }
#elif defined(MODE_BUILD_COEFFICIENTS)
    const uint2 sample_index = uint2(dispatchThreadID.xy);

    const float2 uv = (float2(sample_index) + 0.5) / float2(NUM_SAMPLES_X, NUM_SAMPLES_Y);
    const float2 sample_point = uv * 2.0 - 1.0;
    const float3 dir = normalize(DecodeOctahedralCoord(sample_point));

    float4 albedo = SAMPLE_TEXTURE_CUBE_LOD(sampler_linear, cubemap_color, dir, 0.0);
    float4 color = albedo;

    float sh_values[27];
    ProjectOntoSH9Color(dir, color.rgb, sh_values);

    float3 d_unnorm = float3(sample_point.x, sample_point.y, 1.0 - abs(sample_point.x) - abs(sample_point.y));
    float len = length(d_unnorm);
    float weight = 1.0 / max(len * len * len, 0.0001);

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

    static const float s_aOverPi[9] = {
        1.0,
        2.0/3.0, 2.0/3.0, 2.0/3.0,
        0.25, 0.25, 0.25, 0.25, 0.25
    };

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
        float normFactor = (4.0 * HYP_FMATH_PI) / max(weight, 0.0001);
        env_probes[env_probe_index].sh[i] = float4(env_probes[env_probe_index].sh[i].rgb * normFactor * s_aOverPi[i], 1.0);
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
            total_weight += sh_tiles[(sample_x * NUM_SAMPLES_Y) + sample_y].coeffs_weights[0].a;

            for (int i = 0; i < 9; i++)
            {
                sh_result[i] += sh_tiles[(sample_x * NUM_SAMPLES_Y) + sample_y].coeffs_weights[i].rgb;
            }
        }
    }

    for (int i = 0; i < 9; i++)
    {
        float3 result = sh_result[i] * ((4.0 * HYP_FMATH_PI) / total_weight) * s_aOverPi[i];

        env_probes[env_probe_index].sh[i] = float4(result, 1.0);
    }
#endif

    // // temp: debug
    // for (int i = 0; i < 9; i++)
    // {
    //     env_probes[env_probe_index].sh[i] = float4(1.0, 0.0, 0.0, 1.0);
    // }
#endif
}
