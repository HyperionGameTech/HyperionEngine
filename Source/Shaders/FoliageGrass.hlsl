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

#include "Include/GeometryPass/FoliageCommon.hlsli"
#include "Include/GeometryPass/GeometryPassShading.hlsli"

void ApplyGroundNormal(PSInput input, inout GeometrySurface surface)
{
    const float groundNormalBlend = input.ground_normal_blend;

    if (groundNormalBlend > 0.0)
    {
        surface.N = normalize(lerp(surface.N, normalize(input.ground_normal), groundNormalBlend));
        surface.perceptualRoughness = lerp(surface.perceptualRoughness, 1.0, saturate(groundNormalBlend * 1.5));
    }
}

PSOutput PSMain(PSInput input)
{
    ApplyFoliageBackFace(input);

    GeometrySurface surface = SampleGeometrySurface(input, GetFoliageBackFaceSign(input));
    ApplyGroundNormal(input, surface);
    FinishGeometrySurface(surface);

    PSOutput output = WriteGeometryOutput(input, surface);
    WriteFoliageOutput(surface, output);

#ifndef SHADING_TYPE_LIGHTMAPPED
    if (any(CURRENT_MATERIAL.ground_cover != 0.0))
    {
        output.gbuffer_material |= GBUFFER_GROUND_COVER_BIT;
    }
#endif // !SHADING_TYPE_LIGHTMAPPED

    return output;
}
