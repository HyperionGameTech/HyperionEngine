#ifndef HYP_ALPHA_CUTOUT_HLSLI
#define HYP_ALPHA_CUTOUT_HLSLI

#include "Noise.hlsli"

#ifdef PIXEL_SHADER
float AlphaCutoutCoverage(float alpha, float alphaThreshold, float mipLevel)
{
    const float sharpenedCoverage = saturate((alpha - alphaThreshold) / max(fwidth(alpha), 1e-4) + 0.5);

    return lerp(sharpenedCoverage, alpha, saturate(mipLevel));
}
#endif // PIXEL_SHADER

uint AlphaCutoutSeed(float4x4 modelMatrix, uint vertexId)
{
    const uint instanceHash = pcg_hash(asuint(modelMatrix[0][3]) ^ pcg_hash(asuint(modelMatrix[1][3]) ^ pcg_hash(asuint(modelMatrix[2][3]))));

    return pcg_hash(vertexId ^ instanceHash);
}

bool ShouldDiscardCutout(float coverage, float noise, uint seed)
{
    const float seedOffset = float(seed >> 8u) * (1.0 / 16777216.0);

    return coverage <= frac(noise + seedOffset);
}

#endif // HYP_ALPHA_CUTOUT_HLSLI
