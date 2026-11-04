#include "Include/Defines.hlsli"
#include "Include/Shared.hlsli"
#include "Include/Scene.hlsli"

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

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

DECLARE_SRV(Tonemap, DeferredResult) Texture2D DeferredResult;
DECLARE_SRV(Tonemap, BloomResultTexture) Texture2D BloomResultTexture;
DECLARE_SRV(Tonemap, DebugOverlayTexture) Texture2D DebugOverlayTexture;

DECLARE_SAMPLER(Tonemap, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(Tonemap, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(Tonemap, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(Tonemap, EyeAdaptationBuffer) StructuredBuffer<float4> EyeAdaptationBuffer;

#include "Include/Tonemap.hlsli"

#define TEMPORAL_BLENDING_REVERSE_TONEMAP
#include "Include/Temporal.hlsli"

#define SHARPEN_LOBE_LIMIT (0.25 - 1.0 / 16.0)

float3 LoadSharpenTap(int2 coord, int2 dimensions)
{
    return TemporalTonemap(max(DeferredResult.Load(int3(clamp(coord, int2(0, 0), dimensions - 1), 0)).rgb, 0.0));
}

// AMD's robust contrast-adaptive sharpening, run in a reversible tonemap so HDR highlights don't ring
float3 SharpenSceneColor(float2 texcoord, float sharpness)
{
    uint width, height;
    DeferredResult.GetDimensions(width, height);

    const int2 dimensions = int2(width, height);
    const int2 center_coord = min(int2(texcoord * float2(dimensions)), dimensions - 1);

    const float3 center = LoadSharpenTap(center_coord, dimensions);
    const float3 north = LoadSharpenTap(center_coord + int2(0, -1), dimensions);
    const float3 west = LoadSharpenTap(center_coord + int2(-1, 0), dimensions);
    const float3 east = LoadSharpenTap(center_coord + int2(1, 0), dimensions);
    const float3 south = LoadSharpenTap(center_coord + int2(0, 1), dimensions);

    const float3 min_neighbor = min(min(north, south), min(west, east));
    const float3 max_neighbor = max(max(north, south), max(west, east));

    const float3 hit_min = min(min_neighbor, center) / max(4.0 * max_neighbor, 1e-5);
    const float3 hit_max = (1.0 - max(max_neighbor, center)) / min(4.0 * min_neighbor - 4.0, -1e-5);
    const float3 lobe_rgb = max(-hit_min, hit_max);

    const float lobe = max(-SHARPEN_LOBE_LIMIT, min(max(lobe_rgb.r, max(lobe_rgb.g, lobe_rgb.b)), 0.0)) * sharpness;

    const float3 sharpened = (lobe * (north + west + east + south) + center) / (4.0 * lobe + 1.0);

    return TemporalTonemapInverse(saturate(sharpened));
}

float4 PSMain(PSInput input) : SV_Target0
{
    float2 texcoord = input.texcoord;

    float4 shaded_result;

    if (world_shader_data.taa_params.y > 0.0)
    {
        shaded_result = float4(SharpenSceneColor(texcoord, world_shader_data.taa_params.y), 1.0);
    }
    else
    {
        shaded_result = SAMPLE_TEXTURE_2D(sampler_linear, DeferredResult, texcoord);
    }
    float4 bloom_result = SAMPLE_TEXTURE_2D(sampler_linear, BloomResultTexture, texcoord);

    float4 color_with_bloom = shaded_result + bloom_result;
    color_with_bloom.rgb *= EyeAdaptationBuffer[0].y;

    float3 graded_color = ApplyColorGrading(color_with_bloom.rgb, world_shader_data);

    const float2 vignette_offset = (texcoord - 0.5) * float2(1.0, 0.75);
    graded_color *= lerp(1.0, saturate(1.0 - dot(vignette_offset, vignette_offset) * 1.6), world_shader_data.exposure_grading.w);

    float4 color_output = float4(Tonemap(graded_color, world_shader_data.tonemap_operator), 1.0);

    // debug draws are premultiplied over a transparent clear, alpha being how much of the scene they cover
    const float4 debug_overlay = SAMPLE_TEXTURE_2D(sampler_linear, DebugOverlayTexture, texcoord);
    color_output.rgb = saturate(debug_overlay.rgb) + color_output.rgb * (1.0 - saturate(debug_overlay.a));

#ifdef OUTPUT_PQ_HDR
    const float peakNits = 1000.0;
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif

    return color_output;
}

#endif // PIXEL_SHADER
