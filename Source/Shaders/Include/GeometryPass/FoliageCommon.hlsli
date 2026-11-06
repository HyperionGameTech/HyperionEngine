#ifndef HYP_FOLIAGE_COMMON_HLSLI
#define HYP_FOLIAGE_COMMON_HLSLI

#include "Include/GeometryPass/GeometryPassSurface.hlsli"
#include "Include/FoliageLighting.hlsli"

// back faces turn their normal around, optionally keeping some of it so a leaf card still reads as volume
void ApplyFoliageBackFace(inout PSInput input)
{
    const float3 faceCross = cross(ddx(input.position.xyz), ddy(input.position.xyz));
    const float faceCrossLengthSq = dot(faceCross, faceCross);

    if (input.is_front_face)
    {
        return;
    }

    const float normalBlend = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_FOLIAGE_NORMAL_BLEND);
    const float backfaceVolume = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_FOLIAGE_BACKFACE_VOLUME);

    const float3 vertexNormal = normalize(input.normal);
    float3 turnedNormal = -vertexNormal;

    if (backfaceVolume > 0.0 && faceCrossLengthSq > 1e-20)
    {
        const float3 faceNormal = faceCross * rsqrt(faceCrossLengthSq);
        const float3 keptNormal = normalize(vertexNormal - 2.0 * (1.0 - normalBlend) * dot(vertexNormal, faceNormal) * faceNormal);

        const float3 blendedNormal = lerp(-vertexNormal, keptNormal, backfaceVolume);
        const float blendedLengthSq = dot(blendedNormal, blendedNormal);

        turnedNormal = blendedLengthSq > 1e-8 ? blendedNormal * rsqrt(blendedLengthSq) : keptNormal;
    }

    input.normal = turnedNormal;
    input.bitangent = -input.bitangent;
}

// flipped back faces mirror the whole perturbation, like the underside of a thin sheet
float GetFoliageBackFaceSign(PSInput input)
{
    return input.is_front_face ? 1.0 : -1.0;
}

// transmission on foliage == light going through the leaf. not refraction
float GetRefractionTransmission(GeometrySurface surface)
{
    return 0.0;
}

float3 EvaluateDirectLight(GeometrySurface surface, float3 N, float3 V, float3 L, float NdotL, float3 diffuseLobe, float3 specularLobe, float3 energyCompensation)
{
    return diffuseLobe * FoliageWrapDiffuse(dot(N, L))
        + specularLobe * energyCompensation * NdotL * FoliageSpecularWeight(N, V)
        + diffuseLobe * FoliageTransmission(N, V, L, surface.transmission);
}

void WriteFoliageOutput(GeometrySurface surface, inout PSOutput output)
{
#ifndef SHADING_TYPE_LIGHTMAPPED
    output.gbuffer_material |= GBufferPackTransmission(surface.transmission);
#endif // !SHADING_TYPE_LIGHTMAPPED
}

#endif // HYP_FOLIAGE_COMMON_HLSLI
