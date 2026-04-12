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
    float4 out_color : SV_Target0;
};

DECLARE_BUFFER_DYNAMIC(RenderSSR, CBuffer) cbuffer CBuffer
{
    SSRConstants ssrConstants;
};

DECLARE_SRV(RenderSSR, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(RenderSSR, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(RenderSSR, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;
DECLARE_SRV(RenderSSR, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(RenderSSR, GBufferDepthTexture) Texture2D gbuffer_depth_texture;
DECLARE_SRV(RenderSSR, DeferredResult) Texture2D gbuffer_deferred_result;

DECLARE_SAMPLER(RenderSSR, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RenderSSR, SamplerLinear) SamplerState sampler_linear;
DECLARE_SRV(RenderSSR, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SRV(RenderSSR, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV_DYNAMIC(RenderSSR, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Gbuffer.hlsli"
#include "../include/BlueNoise.hlsli"
#include "../include/Temporal.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define MAX_ROUGHNESS 0.4

bool TraceRays(
    float3 ray_origin,
    float3 ray_direction,
    float jitter,
    float surface_roughness,
    out float2 hit_pixel,
    out float3 hit_point,
    out float hit_weight,
    out float num_iterations)
{
    ray_direction = normalize(ray_direction);
    float3 currStep = ssrConstants.ray_step * ray_direction;
    float3 currPosition = ray_origin;
    
    const int max_iterations = int(ssrConstants.num_iterations);
    
    num_iterations = 0.0;
    hit_weight = 0.0;
    hit_pixel = float2(0.0, 0.0);
    hit_point = float3(0.0, 0.0, 0.0);

    int i = 0;
    for (; i < max_iterations; i++)
    {
        currPosition += currStep;

        hit_pixel = GetProjectedPositionFromView(camera.projection, currPosition);
        
        if (hit_pixel.x != saturate(hit_pixel.x) || hit_pixel.y != saturate(hit_pixel.y)) return false;
        
        float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_pixel).r;
        float4 view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_pixel, depth);

        float step_delta = currPosition.z - view_space_position.z;
        num_iterations += 1.0;

        if (ssrConstants.max_ray_distance > 0.0)
        {
            float traveled = distance(ray_origin, currPosition);
            if (traveled > ssrConstants.max_ray_distance) break;
        }

        if (step_delta > 0.0)
        {
            for (int j = 0; j < 4; j++)
            {
                currStep *= 0.5;
                currPosition -= currStep * sign(step_delta);

                hit_pixel = GetProjectedPositionFromView(camera.projection, currPosition);
                depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, hit_pixel).r;
                view_space_position = ReconstructViewSpacePositionFromDepth(camera.invProjMat, hit_pixel, depth);

                step_delta = currPosition.z - view_space_position.z;

                if (abs(step_delta) < ssrConstants.distance_bias)
                {
                    hit_point = view_space_position.xyz;
                    return true;
                }
            }

            hit_point = view_space_position.xyz;
            return true;
        }
    }

    return false;
}

float CalculateAlpha(
    float num_iterations,
    float2 hit_pixel,
    float3 hit_point,
    float dist,
    float3 ray_direction)
{
    float alpha = 1.0;
    alpha *= 1.0 - (num_iterations / ssrConstants.num_iterations);

    float2 hit_pixel_ndc = hit_pixel * 2.0 - 1.0;
    float max_dimension = saturate(max(abs(hit_pixel_ndc.x), abs(hit_pixel_ndc.y)));
    alpha *= 1.0 - max(0.0, max_dimension - ssrConstants.screen_edge_fade_start) / (1.0 - ssrConstants.screen_edge_fade_end);

    return alpha;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const uint2 coord = uint2(input.position_cs.xy);
    const float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    gbuffer_material_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;
    
    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;
    const float perceptualRoughness = sqrt(roughness);

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;

    if (depth > 0.99999 || roughness > MAX_ROUGHNESS)
    {
        output.out_color = (float4)0.0;
        return output;
    }

    float3 N = GBufferUnpackNormal(normalSample);

    float3 P = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    float3 V = normalize(-P);
    float3 view_space_normal = normalize(mul(camera.view, float4(N, 0.0)).xyz);

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(view_space_normal, tangent, bitangent);

    const float2 texel_size = float2(1.0, 1.0) / float2(ssrConstants.dimension.xy);
    const float texel_size_max = max(texel_size.x, texel_size.y);

    float3 ray_origin;

#define NUM_SAMPLES 32
    float2 rnd = float2(
        SampleBlueNoise(int(coord.x), int(coord.y), int(world_shader_data.frame_counter % NUM_SAMPLES) * 2, NUM_SAMPLES * 2),
        SampleBlueNoise(int(coord.x), int(coord.y), int(world_shader_data.frame_counter % NUM_SAMPLES) * 2 + 1, NUM_SAMPLES * 2));
#ifdef ROUGHNESS_SCATTERING
    float3 H = ImportanceSampleGGX(rnd, view_space_normal, perceptualRoughness);
    H = tangent * H.x + bitangent * H.y + view_space_normal * H.z;
    H = normalize(H);

    float3 ray_direction = reflect(-V, H);
#else
    float3 ray_direction = reflect(-V, view_space_normal);
#endif

    ray_origin = P + ray_direction * 0.001;

    if (dot(ray_direction, -V) < 0.0)
    {
        output.out_color = (float4)0.0;
        return output;
    }

    float2 hit_pixel;
    float3 hit_point;
    float hit_weight;
    float num_iterations;

    bool intersect = TraceRays(ray_origin, ray_direction, rnd.x, perceptualRoughness, hit_pixel, hit_point, hit_weight, num_iterations);

    float dist = distance(ray_origin, hit_point);

    float alpha = CalculateAlpha(num_iterations, hit_pixel, hit_point, dist, ray_direction) * float(intersect);

    alpha *= float(hit_pixel.x == saturate(hit_pixel.x) && hit_pixel.y == saturate(hit_pixel.y));
    alpha *= 1.0 - (roughness / MAX_ROUGHNESS);

    hit_pixel = saturate(hit_pixel);
    hit_pixel *= float(alpha > HYP_FMATH_EPSILON);

    output.out_color = float4(hit_pixel, alpha, 1.0);

    return output;
}

#endif // PIXEL_SHADER
