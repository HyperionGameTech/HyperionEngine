#include "../include/Defines.hlsli"

PERMUTE(CONE_TRACING);
PERMUTE(ROUGHNESS_SCATTERING);

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
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.normal = input.a_normal;
    output.texcoord = input.a_texcoord0;

    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#include "SSRShared.hlsli"
#include "../include/Defines.hlsli"
#include "../include/Noise.hlsli"
#include "../include/BRDF.hlsli"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    uint mask : SV_Target0;
};

DECLARE_BUFFER_DYNAMIC(RenderSSR, CBuffer) cbuffer CBuffer
{
    SSRConstants ssrConstants;
    Camera camera;

    uint frameCounter;
};

DECLARE_SRV(RenderSSR, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(RenderSSR, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(RenderSSR, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;
DECLARE_SRV(RenderSSR, GBufferMipChain) Texture2D GBufferMipChain;
DECLARE_SRV(RenderSSR, HiZTexture) Texture2D HiZTexture;

DECLARE_SAMPLER(RenderSSR, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RenderSSR, SamplerLinear) SamplerState sampler_linear;
DECLARE_SRV(RenderSSR, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Gbuffer.hlsli"
#include "../include/BlueNoise.hlsli"
#include "../include/Temporal.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define MAX_ROUGHNESS 0.4
#define HIZ_STOP_LEVEL 0.0

/// https://maorachow.github.io/3d/graphics/2024/09/11/a-high-performance-screen-space-reflection-algorithm.html

float3 GetTextureSpaceRayPoint(float3 view_space_position)
{
    float4 clip_position = mul(camera.projection, float4(view_space_position, 1.0));
    clip_position.xyz /= clip_position.w;

    return float3(clip_position.x * 0.5 + 0.5, clip_position.y * -0.5 + 0.5, clip_position.z);
}

float2 HiZCellCount(float level, float2 hiz_dimensions)
{
    return floor(hiz_dimensions / exp2(level));
}

float2 HiZCell(float2 pos, float2 cell_count)
{
    return floor(pos * cell_count);
}

float3 HiZIntersectCellBoundary(float3 pos, float3 dir, float2 cell_id, float2 cell_count, float2 cross_step, float2 cross_offset)
{
    float2 cell_size = 1.0 / cell_count;
    float2 planes = (cell_id + cross_step) * cell_size + cross_offset;

    float2 dir_xy = select(abs(dir.xy) < HYP_FMATH_EPSILON, HYP_FMATH_EPSILON * sign(dir.xy + HYP_FMATH_EPSILON), dir.xy);
    float2 solutions = (planes - pos.xy) / dir_xy;

    return pos + dir * max(min(solutions.x, solutions.y), 0.0);
}

#define THICKNESS_DEPTH_RATIO 0.015
#define THICKNESS_ROUGHNESS_MULT 2.0
#define THICKNESS_STEP_MULT 1.1

bool TraceRays(
    float3 ray_origin,
    float3 ray_direction,
    float jitter,
    float surface_roughness,
    out float2 hit_pixel,
    out float3 hit_point,
    out float num_iterations)
{
    hit_pixel = float2(0.0, 0.0);
    hit_point = float3(0.0, 0.0, 0.0);
    num_iterations = 0.0;

    ray_direction = normalize(ray_direction);

    const float maxDistance = ssrConstants.max_ray_distance > 0.0
        ? ssrConstants.max_ray_distance
        : ssrConstants.ray_step * ssrConstants.num_iterations;

    float3 rayEndViewSpace = ray_origin + ray_direction * maxDistance;
    
    // Check if the ray would point back towards the camera.
    if (ray_direction.z < 0.0)
    {
        float t_near = (camera.near - ray_origin.z) / ray_direction.z;

        if (t_near > 0.0)
        {
            rayEndViewSpace = ray_origin + ray_direction * min(t_near, maxDistance);
        }
    }

    // Project into texture space
    const float3 rayStartTex = GetTextureSpaceRayPoint(ray_origin);
    const float3 rayEndTex = GetTextureSpaceRayPoint(rayEndViewSpace);

    float3 rayDirTex = rayEndTex - rayStartTex;

    // Early bail.
    if (abs(rayDirTex.z) < HYP_FMATH_EPSILON)
    {
        return false;
    }

    const bool isForwardRay = rayDirTex.z > 0.0;

    // @TODO Move to constants! GetDimensions() has non-zero cost
    uint2 hizDimensions;
    uint numHizLevels;
    HiZTexture.GetDimensions(0, hizDimensions.x, hizDimensions.y, numHizLevels);

    const float2 hizDimensionsF = float2(hizDimensions);
    const float maxLevel = float(numHizLevels) - 1.0;
    
    const float2 pixelDelta = rayDirTex.xy * hizDimensionsF;
    const float maxPixelDelta = max(abs(pixelDelta.x), abs(pixelDelta.y));
    rayDirTex /= max(maxPixelDelta, 1.0);

    const float2 crossStep = float2(rayDirTex.x >= 0.0 ? 1.0 : 0.0, rayDirTex.y >= 0.0 ? 1.0 : 0.0);
    const float2 crossOffset = float2(rayDirTex.x >= 0.0 ? 1.0 : -1.0, rayDirTex.y >= 0.0 ? 1.0 : -1.0)
        * (1.0 / hizDimensionsF) * 0.25;

    float level = HIZ_STOP_LEVEL;
    
    float3 rayPos = rayStartTex + rayDirTex * (1.0 + jitter);

    float3 currPosition = (float3)0.0;
    float step_delta = 0.0;
    bool found = false;

    float prev_step_delta = 0.0;
    float3 prev_ray_pos = rayPos - rayDirTex;
    float3 prev_ray_position_view = ReconstructViewSpacePositionFromDepth(camera.invProjMat, prev_ray_pos.xy, prev_ray_pos.z).xyz;

    bool hasBracket = false;

    const int max_iterations = int(ssrConstants.num_iterations);

    int i = 0;
    for (; i < max_iterations; i++)
    {
        num_iterations += 1.0;

        if (rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0 || rayPos.z < 0.0 || rayPos.z > 1.0)
        {
            break;
        }

        const float2 cellCount = HiZCellCount(level, hizDimensionsF);
        const float2 cellId = HiZCell(rayPos.xy, cellCount);

        const float2 cellMinMax = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, HiZTexture, (cellId + 0.5) / cellCount, level).rg;
        
        const float cellBoundDepth = isForwardRay ? min(cellMinMax.x, cellMinMax.y) : max(cellMinMax.x, cellMinMax.y);

        const float t = (cellBoundDepth - rayPos.z) / rayDirTex.z;
        const float3 depthPlaneHit = rayPos + rayDirTex * max(t, 0.0);

        const float2 newCellId = HiZCell(depthPlaneHit.xy, cellCount);

        if (int(newCellId.x) != int(cellId.x) || int(newCellId.y) != int(cellId.y))
        {
            rayPos = HiZIntersectCellBoundary(rayPos, rayDirTex, cellId, cellCount, crossStep, crossOffset);
            level = min(maxLevel, level + 1.0);
            continue;
        }

        rayPos = depthPlaneHit;

        if (level > HIZ_STOP_LEVEL)
        {
            level = max(HIZ_STOP_LEVEL, level - 1.0);
            continue;
        }
        
        currPosition = ReconstructViewSpacePositionFromDepth(camera.invProjMat, rayPos.xy, rayPos.z).xyz;

        float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, HiZTexture, rayPos.xy, 0).r;
        float4 view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, rayPos.xy, depth);

        step_delta = currPosition.z - view_space_position.z;

        const float step_view_depth_delta = abs(currPosition.z - prev_ray_position_view.z);
        const float depth_thickness = abs(currPosition.z) * THICKNESS_DEPTH_RATIO;
        const float roughness_mult = lerp(1.0, THICKNESS_ROUGHNESS_MULT, surface_roughness);

        const float effective_thickness = max(ssrConstants.thickness, max(depth_thickness, step_view_depth_delta * THICKNESS_STEP_MULT)) * roughness_mult;

        if (step_delta > 0.0 && step_delta < effective_thickness)
        {
            found = true;
            hasBracket = prev_step_delta <= 0.0;

            if (hasBracket)
            {
                const float t = prev_step_delta / (prev_step_delta - step_delta);

                const float3 crossingRayPos = lerp(prev_ray_pos, rayPos, t);

                depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, HiZTexture, crossingRayPos.xy, 0).r;
                view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, crossingRayPos.xy, depth);

                hit_pixel = crossingRayPos.xy;
            }
            else
            {
                hit_pixel = rayPos.xy;
            }

            hit_point = view_space_position.xyz;

            break;
        }

        prev_step_delta = step_delta;
        prev_ray_pos = rayPos;
        prev_ray_position_view = currPosition;

        rayPos += rayDirTex;
    }

    if (!found)
    {
        return false;
    }

    if (!hasBracket)
    {
        return true;
    }

    // Binary search
    float3 p0 = prev_ray_position_view;
    float3 p1 = currPosition;

    const float initial_span = distance(p0, p1);

    float4 scene_view_position;

    for (int j = 0; j < 4; j++)
    {
        const float3 midpoint = (p0 + p1) * 0.5;

        hit_pixel = GetProjectedPositionFromView(camera.projection, midpoint);
        float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, HiZTexture, hit_pixel, 0).r;
        scene_view_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_pixel, depth);

        const float midpoint_delta = midpoint.z - scene_view_position.z;

        if (abs(midpoint_delta) > initial_span)
        {
            break;
        }

        if (midpoint_delta > 0.0)
        {
            p1 = midpoint;
        }
        else
        {
            p0 = midpoint;
        }

        if (abs(midpoint_delta) < ssrConstants.distance_bias)
        {
            break;
        }
    }

    hit_point = scene_view_position.xyz;

    return true;
}

float CalculateAlpha(
    float numIterations,
    float2 hitPixel,
    float3 hitPoint,
    float dist,
    float3 dir)
{
    float alpha = 1.0;
    alpha *= saturate(1.0 - (numIterations / ssrConstants.num_iterations));

    const float2 distFromCenter = abs(hitPixel * 2.0 - 1.0);
    const float2 edgeFade = 1.0 - smoothstep(ssrConstants.screenEdgeFadeStart, ssrConstants.screenEdgeFadeEnd, distFromCenter);
    alpha *= edgeFade.x * edgeFade.y;

    return alpha;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const uint2 coord = uint2(input.position_cs.xy);
    const float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    GBufferMaterialTexture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    uint2 pixelCoord = clamp(uint2(texcoord * max(0, int2(gbufferDimensions))), 0, int2(gbufferDimensions) - 1);

    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferNormalsTexture, texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, 0 /* don't need mask */, materialParams);

    const float roughness = materialParams.roughness;
    const float perceptualRoughness = sqrt(roughness);

    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, HiZTexture, texcoord, 0).r;

    if (MAX_ROUGHNESS < 1.0 && perceptualRoughness > MAX_ROUGHNESS)
    {
        output.mask = 0;
        return output;
    }

    float3 N = GBufferUnpackNormal(normalSample);

    float3 P = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    float3 V = normalize(-P);
    float3 view_space_normal = normalize(mul(camera.view, float4(N, 0.0)).xyz);

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(view_space_normal, tangent, bitangent);

    float3 ray_origin;

#define NUM_SAMPLES 32
    float2 rnd = float2(
        SampleBlueNoise(int(coord.x), int(coord.y), int(frameCounter % NUM_SAMPLES) * 2, NUM_SAMPLES * 2),
        SampleBlueNoise(int(coord.x), int(coord.y), int(frameCounter % NUM_SAMPLES) * 2 + 1, NUM_SAMPLES * 2));
#ifdef ROUGHNESS_SCATTERING
    float3 H = ImportanceSampleGGX(rnd, view_space_normal, roughness);
    H = tangent * H.x + bitangent * H.y + view_space_normal * H.z;
    H = normalize(H);

    float3 ray_direction = reflect(-V, H);
#else
    float3 ray_direction = reflect(-V, view_space_normal);
#endif

#define ORIGIN_NORMAL_BIAS_RATIO 0.001
    const float origin_bias = max(abs(P.z) * ORIGIN_NORMAL_BIAS_RATIO, 1e-3);
    ray_origin = P + view_space_normal * origin_bias + ray_direction * 0.001;

    float2 hit_pixel;
    float3 hit_point;
    float num_iterations;

    bool intersect = TraceRays(ray_origin, ray_direction, rnd.x, perceptualRoughness, hit_pixel, hit_point, num_iterations);

    float dist = distance(ray_origin, hit_point);

    float alpha = CalculateAlpha(num_iterations, hit_pixel, hit_point, dist, ray_direction) * float(intersect);

    alpha *= float(hit_pixel.x == saturate(hit_pixel.x) && hit_pixel.y == saturate(hit_pixel.y));
    alpha *= 1.0 - (perceptualRoughness / MAX_ROUGHNESS);

#define GRAZING_FADE_THRESHOLD 0.05
    const float NdotV = saturate(dot(view_space_normal, V));
    alpha *= smoothstep(0.0, GRAZING_FADE_THRESHOLD, NdotV);

    hit_pixel = saturate(hit_pixel);
    hit_pixel *= float(alpha > HYP_FMATH_EPSILON);

    // 15 bits == U
    // 15 bits == V
    // 2 bits == mask

    output.mask = HYP_QUANTIZE(hit_pixel.x, 15)
        | (HYP_QUANTIZE(hit_pixel.y, 15) << 15)
        | (HYP_QUANTIZE(alpha, 2) << 30);

    return output;
}

#endif // PIXEL_SHADER
