#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerCommon.hlsli"

// Must match GlimmerGroundCoverConstants in GlimmerSurfaceCache.cpp
struct GlimmerGroundCoverConstants
{
    uint4 materials[GLIMMER_GROUND_COVER_LAYERS]; // per splat layer, ~0 where unused or not bound this frame
    float4 weights[GLIMMER_GROUND_COVER_LAYERS];
};

DECLARE_BUFFER_DYNAMIC(GlimmerGroundCover, CBuffer) cbuffer CBuffer
{
    GlimmerGroundCoverConstants constants;
};

DECLARE_SRV(GlimmerGroundCover, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerGroundCover, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_UAV(GlimmerGroundCover, OutGroundCoverAlbedo) RWStructuredBuffer<float4> OutGroundCoverAlbedo;

#include "GlimmerMaterial.hlsli"

// One thread per splat layer: the weighted mean albedo of the plants its ground cover grows
[numthreads(GLIMMER_GROUND_COVER_LAYERS, 1, 1)]
void CSMain(uint layerIndex : SV_GroupIndex)
{
    float3 albedoSum = (float3)0.0;
    float weightSum = 0.0;

    for (uint materialIndex = 0; materialIndex < 4; materialIndex++)
    {
        const uint binding = constants.materials[layerIndex][materialIndex];
        const float weight = constants.weights[layerIndex][materialIndex];

        if (binding == 0xFFFFFFFFu || weight <= 0.0)
        {
            continue;
        }

        albedoSum += GlimmerGetMaterialAverageAlbedo(binding) * weight;
        weightSum += weight;
    }

    // frames where none of the layer's plants are drawn (so their materials aren't bound) keep the last colour
    if (weightSum > 0.0)
    {
        OutGroundCoverAlbedo[layerIndex] = float4(albedoSum / weightSum, 1.0);
    }
}
