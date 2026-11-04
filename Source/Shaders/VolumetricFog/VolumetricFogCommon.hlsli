#ifndef VOLUMETRIC_FOG_COMMON_HLSLI
#define VOLUMETRIC_FOG_COMMON_HLSLI

struct VolumetricFogGrid
{
    /// xyz = cells
    /// w = 1 when the integrated volume can be sampled
    uint4 dimensions;
    
    /// x = near depth
    /// y = far depth
    /// z = log2(far / near)
    /// w = 1 / slices
    float4 depthParams;
    
    /// xyz = this frame's offset inside a cell (-0.5 to 0.5)
    /// w = history weight
    float4 jitter;
    
    /// x = multiple scattering octaves
    /// y = number of fog volumes
    float4 params;
};

struct VolumetricFogVolume
{
    /// w = edge fade in meters
    float4 aabbMin;
    
    /// w = density
    float4 aabbMax;
    
    /// rgb = albedo
    /// a = ambient intensity
    float4 lighting;
    
    /// x = forward g
    /// y = backward g
    /// z = backward share
    /// w = sun intensity
    float4 phase;
};

float VolumetricFogSliceToDepth(VolumetricFogGrid grid, float slice)
{
    if (slice <= 0.0)
    {
        return 0.0;
    }

    return grid.depthParams.x * exp2(slice * grid.depthParams.w * grid.depthParams.z);
}

float VolumetricFogDepthToSlice(VolumetricFogGrid grid, float depth)
{
    const float relative = log2(max(depth, grid.depthParams.x) / grid.depthParams.x) / grid.depthParams.z;

    return relative * float(grid.dimensions.z);
}

float VolumetricFogLinearDepth(float4x4 viewProjection, float3 positionWS)
{
    return abs(mul(viewProjection, float4(positionWS, 1.0)).w);
}

float3 VolumetricFogRayDirection(
    float4x4 inverseProjection,
    float4x4 inverseView, 
    float3 cameraPosition,
    float2 uv)
{
    const float4 ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.5, 1.0);

    float4 positionVS = mul(inverseProjection, ndc);
    positionVS /= positionVS.w;

    const float3 positionWS = mul(inverseView, positionVS).xyz;

    return normalize(positionWS - cameraPosition);
}

float3 VolumetricFogCellPosition(
    float4x4 viewProjection,
    float3 cameraPosition,
    float3 rayDirection,
    float depth)
{
    const float depthPerMeter = max(abs(mul(viewProjection, float4(rayDirection, 0.0)).w), 1e-4);

    return cameraPosition + rayDirection * (depth / depthPerMeter);
}

float3 VolumetricFogUVW(VolumetricFogGrid grid, float2 uv, float depth)
{
    return float3(uv, VolumetricFogDepthToSlice(grid, depth) * grid.depthParams.w);
}

float VolumetricFogPhase(float g, float cosTheta)
{
    const float g2 = g * g;
    const float denominator = pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5);

    return (1.0 - g2) / (4.0 * HYP_FMATH_PI * denominator);
}

/// rgb = in-scatter from the camera
/// a = transmittance from the camera
float3 ApplyVolumetricFogSample(float3 color, float4 fogSample)
{
    return color * fogSample.a + fogSample.rgb;
}

float4 SampleVolumetricFog(
    Texture3D<float4> integratedTexture,
    SamplerState samplerState,
    VolumetricFogGrid grid,
    float4x4 viewProjection,
    float3 positionWS)
{
    if (grid.dimensions.w == 0u)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    const float4 positionCS = mul(viewProjection, float4(positionWS, 1.0));
    const float linearDepth = abs(positionCS.w);

    const float2 ndc = positionCS.xy / max(linearDepth, 1e-4);
    const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);

    // inset by half a cell so a wrapping sampler never blends in the opposite edge
    const float3 halfCell = 0.5 / float3(grid.dimensions.xyz);
    const float3 uvw = clamp(VolumetricFogUVW(grid, uv, linearDepth), halfCell, 1.0 - halfCell);

    return integratedTexture.SampleLevel(samplerState, uvw, 0);
}

#endif // VOLUMETRIC_FOG_COMMON_HLSLI
