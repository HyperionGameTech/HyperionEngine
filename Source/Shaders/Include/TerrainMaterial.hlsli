#ifndef HYP_TERRAIN_MATERIAL
#define HYP_TERRAIN_MATERIAL

// Terrain material texture slots, continuing on from the standard slots in Material.hlsli.
#define MATERIAL_TEXTURE_TerrainSplatMap 6
#define MATERIAL_TEXTURE_TerrainLayer0 7
#define MATERIAL_TEXTURE_TerrainLayer1 8
#define MATERIAL_TEXTURE_TerrainLayer2 9
#define MATERIAL_TEXTURE_TerrainLayer3 10
#define MATERIAL_TEXTURE_TerrainNormal0 11
#define MATERIAL_TEXTURE_TerrainNormal1 12
#define MATERIAL_TEXTURE_TerrainNormal2 13
#define MATERIAL_TEXTURE_TerrainNormal3 14
#define MATERIAL_TEXTURE_TerrainNormalMap 15

// Layer roles: 0 grass, 1 rock, 2 dirt, 3 snow - matching TerrainGenerator::SynthesizeSplatWeights.
#define TERRAIN_LAYER0_SCALE 0.08
#define TERRAIN_LAYER1_SCALE 0.045
#define TERRAIN_LAYER2_SCALE 0.06
#define TERRAIN_LAYER3_SCALE 0.10

// per-layer albedo tint, so a source texture can be pushed toward the look it needs without a recook.
// Shared with the ray tracing path so bounce light matches what the raster surface shows.
#define TERRAIN_LAYER0_TINT float3(1.00, 1.00, 1.00)
#define TERRAIN_LAYER1_TINT float3(0.62, 0.60, 0.56)
#define TERRAIN_LAYER2_TINT float3(0.92, 0.92, 0.88)
// cooked snow sits at the physical top of the albedo range; this brings it under the ceiling so it still shades
#define TERRAIN_LAYER3_TINT float3(0.82, 0.84, 0.88)

#define TERRAIN_SLOPE_BLEND_START 0.25
#define TERRAIN_SLOPE_BLEND_END 0.55

#define TERRAIN_SPLAT_SHARPNESS 1.35

float GetTerrainLayerScale(uint layerIndex)
{
    switch (layerIndex)
    {
    case 0: return TERRAIN_LAYER0_SCALE;
    case 1: return TERRAIN_LAYER1_SCALE;
    case 2: return TERRAIN_LAYER2_SCALE;
    default: return TERRAIN_LAYER3_SCALE;
    }
}

float3 GetTerrainLayerTint(uint layerIndex)
{
    switch (layerIndex)
    {
    case 0: return TERRAIN_LAYER0_TINT;
    case 1: return TERRAIN_LAYER1_TINT;
    case 2: return TERRAIN_LAYER2_TINT;
    default: return TERRAIN_LAYER3_TINT;
    }
}

#endif
