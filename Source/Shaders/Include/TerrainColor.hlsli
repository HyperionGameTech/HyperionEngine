#ifndef HYP_TERRAIN_COLOR
#define HYP_TERRAIN_COLOR

#define TERRAIN_MACRO_NOISE_SCALE 0.012
#define TERRAIN_MACRO_STRENGTH 0.12
#define TERRAIN_FAR_NOISE_SCALE 0.0016
#define TERRAIN_FAR_STRENGTH 0.14

#define TERRAIN_MACRO_HUE_WARM float3(1.06, 1.00, 0.90)
#define TERRAIN_MACRO_HUE_COOL float3(0.92, 0.97, 1.08)

struct TerrainMacroNoise
{
    float macro;
    float far;
};

float TerrainValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float TerrainFbm(float2 p)
{
    float value = 0.0;
    float amplitude = 0.5;

    [unroll]
    for (int octave = 0; octave < 4; octave++)
    {
        value += amplitude * TerrainValueNoise(p);
        p = p * 2.07 + float2(13.1, 5.7);
        amplitude *= 0.5;
    }

    return value;
}

TerrainMacroNoise SampleTerrainMacroNoise(float2 positionXZ)
{
    TerrainMacroNoise noise;
    noise.macro = TerrainFbm(positionXZ * TERRAIN_MACRO_NOISE_SCALE);
    noise.far = TerrainFbm(positionXZ * TERRAIN_FAR_NOISE_SCALE + 117.3);

    return noise;
}

float3 GetTerrainMacroColorGain(TerrainMacroNoise noise)
{
    const float variation = (noise.macro - 0.5) * (TERRAIN_MACRO_STRENGTH * 2.0)
        + (noise.far - 0.5) * (TERRAIN_FAR_STRENGTH * 2.0);

    return max(1.0 + variation, 0.0) * lerp(TERRAIN_MACRO_HUE_COOL, TERRAIN_MACRO_HUE_WARM, saturate(noise.far));
}

#endif
