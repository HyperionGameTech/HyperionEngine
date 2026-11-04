#include "../Include/Defines.hlsli"

#include "VolumetricFogCommon.hlsli"

DECLARE_SRV(VolumetricFog, MediumTexture) Texture3D<float4> MediumTexture;
DECLARE_UAV(VolumetricFog, OutIntegratedTexture) RWTexture3D<float4> OutIntegratedTexture;

DECLARE_BUFFER_DYNAMIC(VolumetricFog, VolumetricFogIntegrateConstants) cbuffer VolumetricFogIntegrateConstants
{
    VolumetricFogGrid grid;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 column = dispatchThreadId.xy;

    if (any(column >= grid.dimensions.xy))
    {
        return;
    }

    float3 inScatter = (float3)0.0;
    float transmittance = 1.0;

    float previousDepth = 0.0;

    for (uint slice = 0; slice < grid.dimensions.z; slice++)
    {
        const uint3 cell = uint3(column, slice);

        const float depth = VolumetricFogSliceToDepth(grid, float(slice) + 0.5);
        const float thickness = depth - previousDepth;

        previousDepth = depth;

        const float4 medium = MediumTexture.Load(int4(cell, 0));
        const float extinction = max(medium.a, 1e-6);

        const float sliceTransmittance = exp(-extinction * thickness);

        // energy-conserving integral of the in-scatter over the slice (Hillaire 2015)
        const float3 sliceInScatter = (medium.rgb - medium.rgb * sliceTransmittance) / extinction;

        inScatter += transmittance * sliceInScatter;
        transmittance *= sliceTransmittance;

        OutIntegratedTexture[cell] = float4(inScatter, transmittance);
    }
}
