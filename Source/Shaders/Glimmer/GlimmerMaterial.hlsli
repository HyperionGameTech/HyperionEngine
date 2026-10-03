#ifndef GLIMMER_MATERIAL_HLSLI
#define GLIMMER_MATERIAL_HLSLI

float4 GlimmerGetMaterialAverageAlbedoAlpha(uint materialIndex)
{
    if (materialIndex == 0xFFFFFFFFu)
    {
        return float4(0.5, 0.5, 0.5, 1.0);
    }

    const Material material = materials[materialIndex];

    float4 albedo = material.albedo;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    if (HAS_TEXTURE(material, DiffuseMap))
    {
        albedo *= textures[NonUniformResourceIndex(material.texture_indices[MATERIAL_TEXTURE_DiffuseMap / 4][MATERIAL_TEXTURE_DiffuseMap % 4])]
            .SampleLevel(glimmerMaterialSampler, float2(0.5, 0.5), 16.0);
    }
#endif

    return albedo;
}

float3 GlimmerGetMaterialAverageAlbedo(uint materialIndex)
{
    return GlimmerGetMaterialAverageAlbedoAlpha(materialIndex).rgb;
}

float3 GlimmerGetMaterialEmissive(uint materialIndex)
{
    if (materialIndex == 0xFFFFFFFFu)
    {
        return (float3)0.0;
    }

    return max(GET_MATERIAL_EMISSIVE(materials[materialIndex]), (float3)0.0);
}

#endif
