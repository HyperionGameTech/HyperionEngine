#include "../Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/Shared.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "VolumetricFogCommon.hlsli"

DECLARE_SAMPLER(VolumetricFog, SamplerLinear) SamplerState SamplerLinear;

DECLARE_SRV(VolumetricFog, LitTexture) Texture3D<float4> LitTexture;
DECLARE_SRV(VolumetricFog, HistoryTexture) Texture3D<float4> HistoryTexture;
DECLARE_UAV(VolumetricFog, OutHistoryTexture) RWTexture3D<float4> OutHistoryTexture;

DECLARE_BUFFER_DYNAMIC(VolumetricFog, VolumetricFogTemporalConstants) cbuffer VolumetricFogTemporalConstants
{
    Camera camera;
    VolumetricFogGrid grid;

    float4x4 previousViewProjection;
    VolumetricFogGrid previousGrid;

    uint4 historyParams; // x = 1 when the history is usable
};

// a cell whose light changes this much between frames (relative) stops trusting its history
static const float ChangeRejectionScale = 4.0;

static const int3 NeighbourOffsets[6] =
{
    int3(1, 0, 0), int3(-1, 0, 0),
    int3(0, 1, 0), int3(0, -1, 0),
    int3(0, 0, 1), int3(0, 0, -1)
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint3 cell = dispatchThreadId;
    
    [branch]
    if (any(cell >= grid.dimensions.xyz))
    {
        return;
    }

    const float4 current = LitTexture.Load(int4(cell, 0));

    [branch]
    if (historyParams.x == 0u)
    {
        OutHistoryTexture[cell] = current;

        return;
    }

    float4 neighbourMin = current;
    float4 neighbourMax = current;

    const int3 lastCell = int3(grid.dimensions.xyz) - 1;

    [unroll]
    for (uint neighbourIndex = 0; neighbourIndex < 6; neighbourIndex++)
    {
        const float4 neighbour = LitTexture.Load(int4(clamp(int3(cell) + NeighbourOffsets[neighbourIndex], (int3)0, lastCell), 0));

        neighbourMin = min(neighbourMin, neighbour);
        neighbourMax = max(neighbourMax, neighbour);
    }

    const float2 uv = (float2(cell.xy) + 0.5) / float2(grid.dimensions.xy);
    const float depth = VolumetricFogSliceToDepth(grid, float(cell.z) + 0.5);

    const float3 rayDirection = VolumetricFogRayDirection(camera.invProjMat, camera.invViewMat, camera.position.xyz, uv);
    const float3 positionWS = VolumetricFogCellPosition(camera.viewProjMat, camera.position.xyz, rayDirection, depth);

    const float4 previousClip = mul(previousViewProjection, float4(positionWS, 1.0));

    if (previousClip.w <= 1e-4)
    {
        OutHistoryTexture[cell] = current;

        return;
    }

    const float2 previousNdc = previousClip.xy / previousClip.w;
    const float2 previousUV = float2(previousNdc.x * 0.5 + 0.5, 0.5 - previousNdc.y * 0.5);
    const float previousSlice = VolumetricFogDepthToSlice(previousGrid, abs(previousClip.w));

    const float3 previousUVW = float3(previousUV, previousSlice / float(previousGrid.dimensions.z));

    if (any(previousUVW < 0.0) || any(previousUVW > 1.0))
    {
        OutHistoryTexture[cell] = current;

        return;
    }

    const float4 history = HistoryTexture.SampleLevel(SamplerLinear, previousUVW, 0);
    const float4 clampedHistory = clamp(history, neighbourMin, neighbourMax);

    const float currentLuma = dot(current.rgb, (float3)(1.0 / 3.0));
    const float historyLuma = dot(history.rgb, (float3)(1.0 / 3.0));
    const float relativeChange = abs(currentLuma - historyLuma) / max(max(currentLuma, historyLuma), 1e-5);

    const float historyWeight = grid.jitter.w * saturate(1.0 - max(relativeChange * ChangeRejectionScale - 1.0, 0.0) * 0.5);

    const float4 result = lerp(current, clampedHistory, historyWeight);

    OutHistoryTexture[cell] = any(HYP_ISNAN(result)) ? current : result;
}
