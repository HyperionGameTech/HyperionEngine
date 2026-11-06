#include "Include/Defines.hlsli"

PERMUTE(SKINNING);
PERMUTE(ALPHA_DISCARD);
PERMUTE(INSTANCING);
PERMUTE(SHADING_TYPE, DEFERRED, FORWARD, LIGHTMAPPED, UNLIT);

// Used in FORWARD_CLUSTERED only
PERMUTE(FORWARD_CLUSTERED);
PERMUTE(RT_GI);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

PERMUTE(FORWARD_SHADING);
STATIC(MAX_LIGHTS, 4)

#include "Include/GeometryPass/GeometryPassSurface.hlsli"

float GetRefractionTransmission(GeometrySurface surface)
{
    return surface.transmission;
}

float3 EvaluateDirectLight(GeometrySurface surface, float3 N, float3 V, float3 L, float NdotL, float3 diffuseLobe, float3 specularLobe, float3 energyCompensation)
{
    return diffuseLobe * NdotL + specularLobe * energyCompensation * NdotL;
}

#include "Include/GeometryPass/GeometryPassShading.hlsli"

PSOutput PSMain(PSInput input)
{
    GeometrySurface surface = SampleGeometrySurface(input, 1.0);
    FinishGeometrySurface(surface);

    return WriteGeometryOutput(input, surface);
}
