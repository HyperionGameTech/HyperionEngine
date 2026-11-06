#ifndef GLIMMER_MATERIAL_HLSLI
#define GLIMMER_MATERIAL_HLSLI

float4 GlimmerGetMaterialAverageAlbedoAlpha(uint materialIndex)
{
    if (materialIndex == 0xFFFFFFFFu)
    {
        return float4(0.5, 0.5, 0.5, 1.0);
    }

    const Material material = materials[materialIndex];

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    float4 albedo = material.albedo;

    if (HAS_TEXTURE(material, DiffuseMap))
    {
        albedo *= textures[NonUniformResourceIndex(material.texture_indices[MATERIAL_TEXTURE_DiffuseMap / 4][MATERIAL_TEXTURE_DiffuseMap % 4])]
            .SampleLevel(glimmerMaterialSampler, float2(0.5, 0.5), 16.0);
    }

    return albedo;
#else
    return material.average_albedo;
#endif
}

float3 GlimmerGetMaterialAverageAlbedo(uint materialIndex)
{
    return GlimmerGetMaterialAverageAlbedoAlpha(materialIndex).rgb;
}

float GlimmerGetMaterialTransmission(uint materialIndex)
{
    if (materialIndex == 0xFFFFFFFFu)
    {
        return 0.0;
    }

    return saturate(GET_MATERIAL_PARAM(materials[materialIndex], MATERIAL_PARAM_TRANSMISSION));
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
