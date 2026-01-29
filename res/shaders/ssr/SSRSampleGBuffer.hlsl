#include "../include/defines.inc"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE(0) float3 a_position : POSITION;
    HYP_ATTRIBUTE(1) float3 a_normal : NORMAL;
    HYP_ATTRIBUTE(2) float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;

    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#include "ssr_header.inc"
#include "../include/defines.inc"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 out_color : SV_Target0;
};

DECLARE_SRV(RenderSSR, UVImage) Texture2D ssr_uv_image;

DECLARE_BUFFER(RenderSSR, UniformBuffer) cbuffer UniformBuffer
{
    SSRUniforms ssrUniforms;
};

DECLARE_SRV(RenderSSR, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(RenderSSR, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(RenderSSR, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;
DECLARE_SRV(RenderSSR, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(RenderSSR, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

DECLARE_SAMPLER(RenderSSR, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RenderSSR, SamplerLinear) SamplerState sampler_linear;
DECLARE_SRV(RenderSSR, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_BUFFER(RenderSSR, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_BUFFER_DYNAMIC(RenderSSR, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#include "../include/noise.inc"
#include "../include/shared.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/gbuffer.inc"
#include "../include/env_probe.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

float IsoscelesTriangleOpposite(float adjacent_length, float cone_theta)
{
    return 2.0 * tan(cone_theta) * adjacent_length;
}

float IsoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0 * h * h;

    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}

float IsoscelesTriangleNextAdjacent(float adjacent_length, float incircle_radius)
{
    return adjacent_length - (incircle_radius * 2.0);
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const uint2 coord = uint2(input.position_cs.xy);
    const float2 texcoord = input.texcoord;

    const float2 ssr_image_dimensions = float2(ssrUniforms.dimension.xy);

    float4 uv_sample = SAMPLE_TEXTURE_2D(sampler_nearest, ssr_uv_image, texcoord);
    const float2 uv = uv_sample.xy;
    const float alpha = uv_sample.z;

    float4 reflection_sample = float4(0.0, 0.0, 0.0, 0.0);
    float roughness = 0.0;

    float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;

    if (depth > 0.99999)
    {
        output.out_color = float4(0.0, 0.0, 0.0, 0.0);
        return output;
    }

    float3 P = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;
    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);
    float3 N = GBufferUnpackNormal(normalSample);
    float3 V = normalize(camera.position.xyz - P);

    if (alpha > HYP_FMATH_EPSILON)
    {
        uint2 gbufferDimensions;
        gbuffer_material_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

        uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

        uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;

        GBufferMaterialParams materialParams;
        GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

        roughness = materialParams.roughness;
        roughness = clamp(roughness, 0.001, 0.999);

        const float perceptual_roughness = sqrt(roughness);

        const float gloss = 1.0 - roughness;
        const float cone_angle = RoughnessToConeAngle(perceptual_roughness) * 0.5;

        const float trace_size = float(max(ssrUniforms.dimension.x, ssrUniforms.dimension.y));
        const float max_mip_level = 9.0;

        float2 sample_texcoord = texcoord;
        int2 sample_coord = int2(sample_texcoord * (ssr_image_dimensions - 1.0) + 0.5);
        
        const float4 hit_data = SAMPLE_TEXTURE_2D(sampler_nearest, ssr_uv_image, sample_texcoord);
        const float2 hit_uv = hit_data.xy;
        const float hit_mask = hit_data.z;
        const float2 delta_p = (hit_uv - texcoord);

        float adjacent_length = length(delta_p);
        float2 adjacent_unit = normalize(delta_p);

        float remaining_alpha = 1.0;
        float gloss_multiplier = gloss;

        float4 accum_color = float4(0.0, 0.0, 0.0, 0.0);

        float2 velocity = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_velocity_texture, texcoord).xy;

#ifdef CONE_TRACING
        for (int i = 0; i < 14; i++)
        {
            const float opposite_length = IsoscelesTriangleOpposite(adjacent_length, cone_angle);
            const float incircle_size = IsoscelesTriangleInRadius(opposite_length, adjacent_length);
            const float2 sample_position = texcoord + adjacent_unit * (adjacent_length - incircle_size);

            const float mip_level = clamp(log2(incircle_size * max(ssr_image_dimensions.x, ssr_image_dimensions.y)), 0.0, max_mip_level);

            float4 current_reflection_sample = SAMPLE_TEXTURE_2D_LOD(sampler_linear, gbuffer_mip_chain, clamp(hit_uv - velocity, float2(0.0, 0.0), float2(1.0, 1.0)), mip_level);
#else
        const float current_radius = length((hit_uv - texcoord) * float2(ssrUniforms.dimension.xy)) * tan(cone_angle);
        const float mip_level = clamp(log2(current_radius), 0.0, max_mip_level);

        float4 current_reflection_sample = SAMPLE_TEXTURE_2D_LOD(sampler_linear, gbuffer_mip_chain, clamp(hit_uv - velocity, float2(0.0, 0.0), float2(1.0, 1.0)), mip_level);
#endif

#ifdef CONE_TRACING
            current_reflection_sample.rgb *= float3(gloss_multiplier, gloss_multiplier, gloss_multiplier);
            current_reflection_sample.a = gloss_multiplier;

            remaining_alpha -= current_reflection_sample.a;

            if (remaining_alpha < 0.0)
            {
                current_reflection_sample.rgb *= (1.0 - abs(remaining_alpha));
            }

            accum_color += current_reflection_sample;

            if (accum_color.a >= 1.0)
            {
                break;
            }

            adjacent_length = IsoscelesTriangleNextAdjacent(adjacent_length, incircle_size);
            gloss_multiplier *= gloss;
        }
#else
        accum_color = current_reflection_sample;
#endif

        reflection_sample = accum_color;
        reflection_sample.a = min(accum_color.a, 1.0);

        reflection_sample.a *= alpha;
    }

    output.out_color = reflection_sample;

    return output;
}

#endif // PIXEL_SHADER
