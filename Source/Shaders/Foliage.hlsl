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

PSOutput PSMain(PSInput input)
{
    ApplyFoliageBackFace(input);

    GeometrySurface surface = SampleGeometrySurface(input, GetFoliageBackFaceSign(input));
    ApplyFoliageBackFaceMaterial(input, surface);
    FinishGeometrySurface(surface);

    PSOutput output = WriteGeometryOutput(input, surface);
    WriteFoliageOutput(surface, output);

    return output;
}
