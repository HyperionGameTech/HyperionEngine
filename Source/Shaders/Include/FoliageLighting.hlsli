#ifndef HYP_FOLIAGE_LIGHTING_HLSLI
#define HYP_FOLIAGE_LIGHTING_HLSLI

#define FOLIAGE_WRAP 0.5
#define FOLIAGE_TRANSMISSION_POWER 4.0

float FoliageWrapDiffuse(float NdotL)
{
    return saturate((NdotL + FOLIAGE_WRAP) / ((1.0 + FOLIAGE_WRAP) * (1.0 + FOLIAGE_WRAP)));
}

float FoliageTransmission(float3 V, float3 L, float transmission)
{
    return pow(saturate(dot(V, -L)), FOLIAGE_TRANSMISSION_POWER) * transmission;
}

#endif // HYP_FOLIAGE_LIGHTING_HLSLI
