STATIC(MAX_LIGHTS, 4)
STATIC(MAX_ENV_PROBES, 4)

#include "../include/Defines.hlsli"
#include "../include/Noise.hlsli"
#include "../include/EnvProbes.hlsli"

#include "./SSRShared.hlsli"

#include "../include/BRDF.hlsli"

// OUTPUT isn't PERMUTE'd so nothing is usually defined; default to float (SSGI.cpp output is RGBA16F)
#if defined(OUTPUT_RGBA8)
    #define OUTPUT_UAV_TYPE unorm float4
#else
    #define OUTPUT_UAV_TYPE float4
#endif


struct SSGIConstants
{
    uint2 dimension;
    float rayStep;
    float maxIterations;

    float distanceBias;
    float thickness;
    float rayStepDepthScale;
    uint numSamples;
    uint numBoundEnvProbes;
};

DECLARE_UAV(SSGI, OutImage) RWTexture2D<OUTPUT_UAV_TYPE> out_image;

DECLARE_BUFFER_DYNAMIC(SSGI, CBuffer) cbuffer CBuffer
{
    SSGIConstants ssgiConstants;
    EnvProbe envProbes[MAX_ENV_PROBES];
};

DECLARE_SRV(SSGI, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;

DECLARE_SRV(SSGI, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(SSGI, DeferredShadingTexture) Texture2D DeferredShadingTexture;

DECLARE_SAMPLER(SSGI, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(SSGI, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(SSGI, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SRV(SSGI, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV_DYNAMIC(SSGI, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/BlueNoise.hlsli"
#include "../include/Octahedron.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(SSGI, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

#define RAY_OFFSET 0.05
#define ENVIRONMENT_INTENSITY 1.0

// amount to 'brighten up' the SSGI result
#define SSGI_INTENSITY 1.1

// iterations for the binary search refinement, separate budget from the ray march (maxIterations)
#define SSGI_REFINEMENT_STEPS 6

bool TraceRays(
    float3 ray_origin,
    float3 ray_direction,
    out float2 hit_uv,
    out float4 hit_view_space_position,
    out float hit_depth,
    out float out_maxIterations)
{
    bool intersect = false;
    out_maxIterations = 0.0;
    hit_uv = float2(0.0, 0.0);
    hit_depth = 1.0;
    hit_view_space_position = float4(0.0, 0.0, 0.0, 0.0);

    ray_direction = normalize(ray_direction);

    float3 marching_position = ray_origin;
    float step_delta = 0.0;
    float step_length = ssgiConstants.rayStep;

    for (int i = 0; i < int(ssgiConstants.maxIterations); i++)
    {
        // grow step length with distance from the camera so distant geometry is covered
        step_length = max(ssgiConstants.rayStep, abs(marching_position.z) * ssgiConstants.rayStepDepthScale);

        marching_position += ray_direction * step_length;

        hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
        hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferDepthTexture, hit_uv).r;
        hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

        step_delta = marching_position.z - hit_view_space_position.z;
        out_maxIterations += 1.0;

        if (step_delta > 0.0)
        {
            if (step_delta > ssgiConstants.thickness)
            {
                // penetrated too far past the surface in one step; likely skipped over
                // thin geometry or landed far behind it
                return false;
            }

            intersect = true;
            break;
        }
    }

    if (!intersect)
    {
        return false;
    }

    // binary search refinement to narrow down the hit position
    for (int i = 0; i < SSGI_REFINEMENT_STEPS; i++)
    {
        step_length *= 0.5;
        marching_position -= ray_direction * step_length * sign(step_delta);

        hit_uv = GetProjectedPositionFromView(camera.projection, marching_position);
        hit_depth = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferDepthTexture, hit_uv).r;
        hit_view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_uv, hit_depth);

        step_delta = marching_position.z - hit_view_space_position.z;

        if (abs(step_delta) < ssgiConstants.distanceBias)
        {
            break;
        }
    }

    return true;
}

float CalculateAlpha(
    float maxIterations,
    float2 hit_uv)
{
    float alpha = 1.0;

    // Fade ray hits that approach the maximum iterations
    alpha *= 1.0 - saturate(maxIterations / max(ssgiConstants.maxIterations, 1.0));

    // Fade ray hits that approach the screen edge
    float2 uvNDC = hit_uv * 2.0 - 1.0;
    float maxDimension = saturate(max(abs(uvNDC.x), abs(uvNDC.y)));
    alpha *= 1.0 - saturate((maxDimension - 0.9) / 0.1);

    return alpha;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint pixel_index = dispatchThreadID.x;
    const uint2 coord = uint2(
        pixel_index % ssgiConstants.dimension.x,
        pixel_index / ssgiConstants.dimension.x);

    if (any(coord >= ssgiConstants.dimension.xy))
    {
        return;
    }

    const float2 texcoord = saturate((float2(coord) + 0.5) / float2(ssgiConstants.dimension.xy));

    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferNormalsTexture, texcoord);

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferDepthTexture, texcoord).r;

    if (depth > 0.9999)
    {
        out_image[coord] = (float4)0.0;
        return;
    }

    const float3 P = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    const float3 N = GBufferUnpackNormal(normalSample);
    const float3 view_space_normal = normalize(mul(camera.view, float4(N, 0.0)).xyz);

    float2 hit_uv;
    float4 hit_view_space_position;
    float hit_depth;
    float maxIterations;

    float4 accum_result = (float4)0.0;

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(view_space_normal, tangent, bitangent);

    const uint numRaySamples = 10; // local (per dispatch) sample count.
    const uint temporalSampleIndex = (world_shader_data.frame_counter % ssgiConstants.numSamples);

    for (uint rayIndex = 0; rayIndex < numRaySamples; rayIndex++)
    {
        const uint sampleIndex = temporalSampleIndex * numRaySamples + rayIndex;

        const float2 rnd = float2(
            SampleBlueNoise(int(coord.x), int(coord.y), int(sampleIndex), 0),
            SampleBlueNoise(int(coord.x), int(coord.y), int(sampleIndex), 1));

        const float3 d = SampleCosineWeightedHemisphere(rnd);

        const float3 ray_direction = normalize(tangent * d.x + bitangent * d.y + view_space_normal * d.z);
        const float3 ray_origin = P + view_space_normal * RAY_OFFSET;

        float alpha = 0.0;
        float4 hit_radiance = (float4)0.0;

        if (TraceRays(ray_origin, ray_direction, hit_uv, hit_view_space_position, hit_depth, maxIterations))
        {
            const float3 hit_normal = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, GBufferNormalsTexture, hit_uv));
            const float3 hit_normal_view = normalize(mul(camera.view, float4(hit_normal, 0.0)).xyz);

            // reject backface hits to reduce light leaking through geometry
            if (dot(hit_normal_view, -ray_direction) > 0.0)
            {
                alpha = CalculateAlpha(maxIterations, hit_uv);
            }

            if (alpha > HYP_FMATH_EPSILON)
            {
                float2 sample_uv = saturate(hit_uv);
                float4 color = SAMPLE_TEXTURE_2D_LOD(sampler_linear, DeferredShadingTexture, sample_uv, 0.0);
                
                hit_radiance = float4(color.rgb, 1.0);
            }
        }

        float4 environment_radiance = (float4)0.0;

        if (alpha < 1.0)
        {
            float3 rayDirWorld = normalize(mul(camera.invViewMat, float4(ray_direction, 0.0)).xyz);

            for (uint envProbeIdx = 0; envProbeIdx < ssgiConstants.numBoundEnvProbes && environment_radiance.a < 1.0; envProbeIdx++)
            {
                const uint envProbeTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(envProbes[envProbeIdx]);

                if (envProbeTextureIndex == INVALID_ENV_PROBE_TEXTURE)
                {
                    continue;
                }

                const float skyLightIntensity = GET_ENV_PROBE_TYPE(envProbes[envProbeIdx]) == EPT_SKY ? world_shader_data.sky_light_params.x : 1.0;

                environment_radiance += EnvProbeSample(sampler_linear, envProbesColorTexture, envProbeTextureIndex, rayDirWorld, 6.0)
                    * ENVIRONMENT_INTENSITY
                    * skyLightIntensity
                    * (1.0 - environment_radiance.a);
            }
        }
        
        accum_result += float4(
            lerp(environment_radiance.rgb, hit_radiance.rgb, alpha),
            hit_radiance.a * alpha);
    }

    out_image[coord] = accum_result / float(numRaySamples) * SSGI_INTENSITY;
}
