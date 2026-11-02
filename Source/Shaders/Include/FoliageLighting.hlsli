#ifndef HYP_FOLIAGE_LIGHTING_HLSLI
#define HYP_FOLIAGE_LIGHTING_HLSLI

#define FOLIAGE_WRAP 0.5
#define FOLIAGE_TRANSMISSION_POWER 4.0
// a canopy is thousands of tiny leaves at random angles - sharp highlights off bent normals read as frost
#define FOLIAGE_MIN_ROUGHNESS 0.6
#define FOLIAGE_SPECULAR_FADE_NDOTV 0.5
#define FOLIAGE_CROWN_EXTINCTION 0.15

float FoliageWrapDiffuse(float NdotL)
{
    return saturate((NdotL + FOLIAGE_WRAP) / ((1.0 + FOLIAGE_WRAP) * (1.0 + FOLIAGE_WRAP)));
}

float FoliageSpecularWeight(float3 N, float3 V)
{
    return smoothstep(0.0, FOLIAGE_SPECULAR_FADE_NDOTV, dot(N, V));
}

float3 FoliageTransmissionTint(float3 albedo)
{
    return albedo / max(max(albedo.r, max(albedo.g, albedo.b)), 0.001);
}

float FoliageTransmission(float3 N, float3 V, float3 L, float transmission, float shadow, float crownTransmittance)
{
    const float backLit = saturate(-dot(N, L)) * max(shadow, crownTransmittance);
    const float forwardScatter = pow(saturate(dot(V, -L)), FOLIAGE_TRANSMISSION_POWER) * shadow;

    return (backLit + forwardScatter) * transmission;
}

float FoliageTransmission(float3 N, float3 V, float3 L, float transmission)
{
    return FoliageTransmission(N, V, L, transmission, 1.0, 0.0);
}

#endif // HYP_FOLIAGE_LIGHTING_HLSLI
