#include "../include/Defines.hlsli"

PERMUTE(CONE_TRACING);

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

#include "SSRShared.hlsli"
#include "../include/Defines.hlsli"

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

DECLARE_SRV(RenderSSR, UVImage) Texture2D<uint> SSRMask;

DECLARE_BUFFER_DYNAMIC(RenderSSR, CBuffer) cbuffer CBuffer
{
    SSRConstants ssrConstants;

    Camera camera;
};

DECLARE_SRV(RenderSSR, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(RenderSSR, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(RenderSSR, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;
DECLARE_SRV(RenderSSR, GBufferMipChain) Texture2D GBufferMipChain;
DECLARE_SRV(RenderSSR, HiZTexture) Texture2D HiZTexture;

DECLARE_SAMPLER(RenderSSR, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RenderSSR, SamplerLinear) SamplerState sampler_linear;
DECLARE_SRV(RenderSSR, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#include "../include/Noise.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/EnvProbes.hlsli"

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

    //const float2 ssr_image_dimensions = float2(ssrConstants.dimension.xy);
    
    uint2 maskDimensions;
    SSRMask.GetDimensions(maskDimensions.x, maskDimensions.y);
    
    const int2 ssrSampleCoord = clamp(int2(texcoord * (float2)maskDimensions), (int2)0, (int2)maskDimensions - 1);

    const uint ssrMask = SSRMask.Load(int3(ssrSampleCoord, 0));
    
    const float2 hitUV = float2(HYP_UNQUANTIZE(ssrMask & 0x7FFFu, 15), HYP_UNQUANTIZE((ssrMask >> 15) & 0x7FFFu, 15));
    const float alpha = HYP_UNQUANTIZE((ssrMask >> 30) & 0x3u, 2);

    float4 reflection_sample = (float4)0;
    float roughness = 0.0;

    float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, HiZTexture, texcoord, 0).r;
    
    float3 P = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;
    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, GBufferNormalsTexture, texcoord);
    float3 N = GBufferUnpackNormal(normalSample);
    float3 V = normalize(camera.position.xyz - P);

    if (alpha > HYP_FMATH_EPSILON)
    {
        uint2 gbufferDimensions;
        GBufferMaterialTexture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

        uint2 pixelCoord = clamp(uint2(int2(texcoord * (float2)gbufferDimensions)), 0, int2(gbufferDimensions) - 1);

        GBufferMaterialParams materialParams;
        GBufferUnpackMaterialParams(normalSample.x, 0 /* don't need mask */, materialParams);

        roughness = materialParams.roughness;
        roughness = clamp(roughness, 0.001, 0.999);

        const float perceptualRoughness = sqrt(roughness);

        const float gloss = 1.0 - perceptualRoughness;
        const float cone_angle = RoughnessToConeAngle(perceptualRoughness) * 0.5;

        const float trace_size = float(max(ssrConstants.dimension.x, ssrConstants.dimension.y));
        
        float max_mip_level = 0.0;
        // calc max mip level
        max_mip_level = log2(max((float)gbufferDimensions.x, (float)gbufferDimensions.y));
        
        const float2 delta_p = saturate(hitUV - texcoord);

        float adjacent_length = length(delta_p);
        float2 adjacent_unit = adjacent_length > HYP_FMATH_EPSILON ? (delta_p / adjacent_length) : float2(0.0, 0.0);

        float remaining_alpha = 1.0;
        float gloss_multiplier = gloss;

        float4 accum_color = float4(0.0, 0.0, 0.0, 0.0);

        float2 velocity = SAMPLE_TEXTURE_2D_LOD(sampler_linear, GBufferVelocityTexture, hitUV, 0).xy;

#ifdef CONE_TRACING
        for (int i = 0; i < 14; i++)
        {
            const float opposite_length = IsoscelesTriangleOpposite(adjacent_length, cone_angle);
            const float incircle_size = IsoscelesTriangleInRadius(opposite_length, adjacent_length);
            const float2 sample_position = texcoord + adjacent_unit * (adjacent_length - incircle_size);

            const float mip_level = clamp(log2(incircle_size * (float)max(gbufferDimensions.x, gbufferDimensions.y)), 0.0, max_mip_level);

            float4 current_reflection_sample = SAMPLE_TEXTURE_2D_LOD(sampler_linear, GBufferMipChain, saturate(hitUV - velocity), mip_level);
            current_reflection_sample = any(isnan(current_reflection_sample)) ? float4(0.0, 0.0, 1.0, 1.0) : current_reflection_sample;
#else
        const float current_radius = length((hitUV - texcoord) * float2(ssrConstants.dimension.xy)) * tan(cone_angle);
        const float mip_level = clamp(log2(current_radius), 0.0, max_mip_level);

        float4 current_reflection_sample = SAMPLE_TEXTURE_2D_LOD(sampler_linear, GBufferMipChain, saturate(hitUV - velocity), mip_level);
#endif

#ifdef CONE_TRACING
            current_reflection_sample.rgb *= gloss_multiplier;
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

        ///reflection_sample.a = min(accum_color.a, 1.0);
        ///reflection_sample = any(isnan(accum_color)) ? float4(0.0, 0.0, 0.0, 1.0) : accum_color;

        reflection_sample = accum_color;
        reflection_sample.a *= alpha;
    }

    output.out_color = reflection_sample;

    return output;
}

#endif // PIXEL_SHADER
