#ifndef HYP_TERRAIN_MATERIAL
#define HYP_TERRAIN_MATERIAL

////////// TEXTURES //////////
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

//////////////////////////////

#define TERRAIN_MAX_ALBEDO 0.88

#define TERRAIN_SPLAT_SHARPNESS 1.35

#define TERRAIN_SLOPE_BLEND_START 0.25
#define TERRAIN_SLOPE_BLEND_END 0.55

#define TERRAIN_SNOW_SHED_SLOPE_START 0.22
#define TERRAIN_SNOW_SHED_SLOPE_END 0.38

/////////// LAYERS ///////////

// 0 = grass, 1 = rock, 2 = dirt, 3 = snow
#define TERRAIN_LAYER0_SCALE 0.08
#define TERRAIN_LAYER1_SCALE 0.045
#define TERRAIN_LAYER2_SCALE 0.06
#define TERRAIN_LAYER3_SCALE 0.10

static const float s_terrainLayerScales[4] = {
    TERRAIN_LAYER0_SCALE,
    TERRAIN_LAYER1_SCALE,
    TERRAIN_LAYER2_SCALE,
    TERRAIN_LAYER3_SCALE
};

#define TERRAIN_LAYER0_TINT float3(0.62, 0.97, 1.45)
#define TERRAIN_LAYER1_TINT float3(0.25, 0.24, 0.225)
#define TERRAIN_LAYER2_TINT float3(0.92, 0.92, 0.88)
#define TERRAIN_LAYER3_TINT float3(0.82, 0.84, 0.88)

static const float3 s_terrainLayerTints[4] = {
    TERRAIN_LAYER0_TINT,
    TERRAIN_LAYER1_TINT,
    TERRAIN_LAYER2_TINT,
    TERRAIN_LAYER3_TINT
};

#define TERRAIN_LAYER0_FIELD_TINT float3(0.88, 1.38, 2.05)
#define TERRAIN_GRASS_FIELD_FADE_START 45.0
#define TERRAIN_GRASS_FIELD_FADE_END 85.0

float GetTerrainLayerScale(uint layerIndex)
{
    return s_terrainLayerScales[min(layerIndex, 3)];
}

float3 GetTerrainLayerTint(uint layerIndex)
{
    return s_terrainLayerTints[min(layerIndex, 3)];
}

float3 GetTerrainLayerTintAtDistance(uint layerIndex, float viewDistance)
{
    const float3 tint = GetTerrainLayerTint(layerIndex);
    return (layerIndex != 0)
        ? tint
        : lerp(tint, TERRAIN_LAYER0_FIELD_TINT, smoothstep(TERRAIN_GRASS_FIELD_FADE_START, TERRAIN_GRASS_FIELD_FADE_END, viewDistance));
}

#endif
