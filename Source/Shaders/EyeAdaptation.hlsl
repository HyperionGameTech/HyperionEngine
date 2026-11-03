#include "Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define EYE_ADAPTATION_GRID 8
#define EYE_ADAPTATION_SAMPLES (EYE_ADAPTATION_GRID * EYE_ADAPTATION_GRID)
#define EYE_ADAPTATION_TAPS 4
#define EYE_ADAPTATION_MIN_LUMINANCE 1e-6
#define EYE_ADAPTATION_MAX_LUMINANCE 65504.0

struct EyeAdaptationConstants
{
    /// x = seconds since the last frame
    /// y = 1 to start over rather than ease
    /// z = ease rate
    float4 params;
};

DECLARE_BUFFER_DYNAMIC(EyeAdaptation, CBuffer) cbuffer CBuffer
{
    EyeAdaptationConstants constants;
};

DECLARE_SRV(EyeAdaptation, SceneColor) Texture2D SceneColor;
DECLARE_SAMPLER(EyeAdaptation, SamplerLinear) SamplerState samplerLinear;

DECLARE_SRV(EyeAdaptation, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

/// x = metered log2 luminance after the scene's exposure
/// y = multiplier
DECLARE_UAV(EyeAdaptation, OutAdaptation) RWStructuredBuffer<float4> OutAdaptation;

groupshared float2 s_weighted[EYE_ADAPTATION_SAMPLES];

[numthreads(EYE_ADAPTATION_GRID, EYE_ADAPTATION_GRID, 1)]
void CSMain(uint3 groupThreadId : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    float2 weighted = float2(0.0, 0.0);

    for (uint tapIndex = 0; tapIndex < EYE_ADAPTATION_TAPS * EYE_ADAPTATION_TAPS; tapIndex++)
    {
        const float2 tap = float2(tapIndex % EYE_ADAPTATION_TAPS, tapIndex / EYE_ADAPTATION_TAPS);
        const float2 uv = (float2(groupThreadId.xy) + (tap + 0.5) / float(EYE_ADAPTATION_TAPS)) / float(EYE_ADAPTATION_GRID);

        const float3 color = SceneColor.SampleLevel(samplerLinear, uv, 0.0).rgb;
        const float luminance = dot(color, float3(0.2126, 0.7152, 0.0722)) * world_shader_data.exposure_grading.x;

        const float2 fromCentre = uv - 0.5;
        const float weight = luminance > EYE_ADAPTATION_MIN_LUMINANCE ? saturate(1.0 - dot(fromCentre, fromCentre) * 2.0) : 0.0;

        weighted += float2(log2(clamp(luminance, EYE_ADAPTATION_MIN_LUMINANCE, EYE_ADAPTATION_MAX_LUMINANCE)) * weight, weight);
    }

    s_weighted[groupIndex] = weighted;

    GroupMemoryBarrierWithGroupSync();

    for (uint stride = EYE_ADAPTATION_SAMPLES / 2; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            s_weighted[groupIndex] += s_weighted[groupIndex + stride];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex != 0)
    {
        return;
    }

    const float4 adaptation = world_shader_data.eye_adaptation_params;
    const float lowLog2 = log2(adaptation.z);
    const float highLog2 = log2(adaptation.w);

    float metered = s_weighted[0].y > 1e-4 ? s_weighted[0].x / s_weighted[0].y : lowLog2;

    const float previous = OutAdaptation[0].x;

    if (constants.params.y == 0.0 && isfinite(previous))
    {
        metered = lerp(previous, metered, 1.0 - exp2(-constants.params.x * constants.params.z));
    }

    if (!isfinite(metered))
    {
        metered = lowLog2;
    }
    
    const float offset = max(lowLog2 - metered, 0.0) + min(highLog2 - metered, 0.0);
    const float stops = clamp(offset * adaptation.x, -adaptation.y, adaptation.y);

    OutAdaptation[0] = float4(metered, exp2(stops), 0.0, 0.0);
}
