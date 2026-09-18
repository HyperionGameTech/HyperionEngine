#ifndef HYP_RAYTRACING_TERRAIN_SURFACE
#define HYP_RAYTRACING_TERRAIN_SURFACE

#include "../TerrainMaterial.hlsli"

// Terrain albedo for ray hits. Ray tracing has no derivatives and the result only feeds indirect lighting,
// so this keeps the splat blend from Terrain.hlsl but drops triplanar, anti tiling, parallax, detail normals
// and the noise breakup. Layers are projected along the dominant normal axis and read from a coarse mip.

#define TERRAIN_RAY_HIT_LOD 4.0

bool IsTerrainMaterial(Material material)
{
    return HAS_TEXTURE(material, TerrainSplatMap) || HAS_TEXTURE(material, TerrainLayer0);
}

#ifdef HYP_FEATURES_BINDLESS_TEXTURES

float2 TerrainRayHitUv(float3 position, float3 normal, float scale)
{
    const float3 absNormal = abs(normal);

    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z)
    {
        return position.xz * scale;
    }

    return absNormal.x >= absNormal.z ? position.zy * scale : position.xy * scale;
}

float3 SampleTerrainLayerAlbedo(Material material, uint layerIndex, float3 position, float3 normal)
{
    Texture2D tex;

    switch (layerIndex)
    {
    case 0: tex = GET_TEXTURE(material, TerrainLayer0); break;
    case 1: tex = GET_TEXTURE(material, TerrainLayer1); break;
    case 2: tex = GET_TEXTURE(material, TerrainLayer2); break;
    default: tex = GET_TEXTURE(material, TerrainLayer3); break;
    }

    const float2 uv = TerrainRayHitUv(position, normal, GetTerrainLayerScale(layerIndex));

    return SAMPLE_TEXTURE_2D_LOD(texture_sampler, tex, uv, TERRAIN_RAY_HIT_LOD).rgb * GetTerrainLayerTint(layerIndex);
}

float3 SampleTerrainAlbedo(Material material, float3 position, float3 normal, float2 splatTexcoord)
{
    float4 weights;

    if (HAS_TEXTURE(material, TerrainSplatMap))
    {
        weights = SAMPLE_TEXTURE_2D_LOD(texture_sampler, GET_TEXTURE(material, TerrainSplatMap), splatTexcoord, 0.0);
        weights = saturate((weights - 0.5) * TERRAIN_SPLAT_SHARPNESS + 0.5);
    }
    else
    {
        const float rockBlend = smoothstep(TERRAIN_SLOPE_BLEND_START, TERRAIN_SLOPE_BLEND_END, saturate(1.0 - normal.y));

        weights = float4(1.0 - rockBlend, rockBlend, 0.0, 0.0);
    }

    const float4 hasLayerTexture = float4(
        float(HAS_TEXTURE(material, TerrainLayer0)),
        float(HAS_TEXTURE(material, TerrainLayer1)),
        float(HAS_TEXTURE(material, TerrainLayer2)),
        float(HAS_TEXTURE(material, TerrainLayer3)));

    // layers with no texture hand their weight to the base layer, matching Terrain.hlsl
    const float missingLayerWeight = dot(weights, 1.0 - hasLayerTexture);

    weights *= hasLayerTexture;
    weights.x += missingLayerWeight * hasLayerTexture.x;

    const float totalWeight = weights.x + weights.y + weights.z + weights.w;

    if (totalWeight <= 0.001)
    {
        return material.albedo.rgb;
    }

    weights /= totalWeight;

    float3 albedo = float3(0.0, 0.0, 0.0);

    if (weights.x > 0.001)
    {
        albedo += weights.x * SampleTerrainLayerAlbedo(material, 0, position, normal);
    }

    if (weights.y > 0.001)
    {
        albedo += weights.y * SampleTerrainLayerAlbedo(material, 1, position, normal);
    }

    if (weights.z > 0.001)
    {
        albedo += weights.z * SampleTerrainLayerAlbedo(material, 2, position, normal);
    }

    if (weights.w > 0.001)
    {
        albedo += weights.w * SampleTerrainLayerAlbedo(material, 3, position, normal);
    }

    return albedo;
}

#endif // HYP_FEATURES_BINDLESS_TEXTURES

#endif
