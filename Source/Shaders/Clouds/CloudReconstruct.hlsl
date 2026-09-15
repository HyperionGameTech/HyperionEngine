#include "../include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// Rebuilds the half resolution cloud image each frame. The trace covers one pixel of every 2x2 block, cycling through
// the block over four frames; the other three come from last frame's image reprojected to where the clouds are now

DECLARE_SRV(Clouds, InTraceTexture) Texture2D InTraceTexture;
DECLARE_SRV(Clouds, InTraceDistanceTexture) Texture2D<float> InTraceDistanceTexture;

DECLARE_SRV(Clouds, InHistoryTexture) Texture2D InHistoryTexture;
DECLARE_SRV(Clouds, InHistoryDistanceTexture) Texture2D<float> InHistoryDistanceTexture;

DECLARE_UAV(Clouds, OutHistoryTexture) RWTexture2D<float4> OutHistoryTexture;
DECLARE_UAV(Clouds, OutHistoryDistanceTexture) RWTexture2D<float> OutHistoryDistanceTexture;

DECLARE_SAMPLER(Clouds, SamplerLinear) SamplerState SamplerLinear;

DECLARE_BUFFER_DYNAMIC(Clouds, CloudReconstructConstants) cbuffer CloudReconstructConstants
{
    Camera camera;

    // last frame's unjittered view projection, kept by the pass rather than the camera so it's never stale
    float4x4 previousViewProjection;

    uint2 traceDimensions;
    uint2 historyDimensions;

    uint2 traceOffset;
    uint historyValid;
    uint _pad0;
};

// how much of a freshly traced pixel replaces its history. Below 1 so the per frame jitter averages out
static const float TracedPixelFreshWeight = 0.5;

// slack on the neighbourhood clamp, so gently evolving clouds don't get their history thrown away
static const float NeighbourhoodClampMargin = 0.05;

float3 GetViewRayDirection(float2 uv)
{
    float4 farPointView = mul(camera.invProjMat, float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0));
    farPointView /= farPointView.w;

    return normalize(mul(camera.invViewMat, float4(farPointView.xyz, 0.0)).xyz);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 pixel = dispatchThreadId.xy;

    if (any(pixel >= historyDimensions))
    {
        return;
    }

    const uint2 tracePixel = min(pixel / 2u, traceDimensions - 1u);
    const uint2 tracedPixelInBlock = min(tracePixel * 2u + traceOffset, historyDimensions - 1u);

    const bool isTracedThisFrame = all(pixel == tracedPixelInBlock);

    const float2 uv = (float2(pixel) + 0.5) / float2(historyDimensions);

    const float4 fresh = InTraceTexture.Load(int3(tracePixel, 0));
    const float freshDistance = InTraceDistanceTexture.Load(int3(tracePixel, 0));

    // where this pixel's clouds were last frame
    const float3 worldPosition = camera.position.xyz + GetViewRayDirection(uv) * freshDistance;

    const float4 previousClip = mul(previousViewProjection, float4(worldPosition, 1.0));
    const float2 previousUV = (previousClip.xy / max(previousClip.w, 1e-5)) * float2(0.5, -0.5) + 0.5;

    const bool hasHistory = historyValid != 0
        && previousClip.w > 0.0
        && all(previousUV >= 0.0)
        && all(previousUV <= 1.0);

    if (!hasHistory)
    {
        OutHistoryTexture[pixel] = InTraceTexture.SampleLevel(SamplerLinear, uv, 0.0);
        OutHistoryDistanceTexture[pixel] = InTraceDistanceTexture.SampleLevel(SamplerLinear, uv, 0.0);

        return;
    }

    float4 neighbourhoodMin = fresh;
    float4 neighbourhoodMax = fresh;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            const int2 neighbourPixel = clamp(int2(tracePixel) + int2(x, y), int2(0, 0), int2(traceDimensions) - 1);
            const float4 neighbour = InTraceTexture.Load(int3(neighbourPixel, 0));

            neighbourhoodMin = min(neighbourhoodMin, neighbour);
            neighbourhoodMax = max(neighbourhoodMax, neighbour);
        }
    }

    const float4 margin = (neighbourhoodMax - neighbourhoodMin) * NeighbourhoodClampMargin + float4(0.0, 0.0, 0.0, 0.01);

    float4 history = InHistoryTexture.SampleLevel(SamplerLinear, previousUV, 0.0);
    history = clamp(history, neighbourhoodMin - margin, neighbourhoodMax + margin);

    const float historyDistance = InHistoryDistanceTexture.SampleLevel(SamplerLinear, previousUV, 0.0);

    if (isTracedThisFrame)
    {
        OutHistoryTexture[pixel] = lerp(history, fresh, TracedPixelFreshWeight);
        OutHistoryDistanceTexture[pixel] = freshDistance;
    }
    else
    {
        OutHistoryTexture[pixel] = history;
        OutHistoryDistanceTexture[pixel] = historyDistance;
    }
}
