#ifndef HYP_SAMPLE_DDGI
#define HYP_SAMPLE_DDGI

#include "./ProbeUniforms.hlsli"
#include "./Shared.hlsli"

float4 DDGISampleCascadeIrradiance(uint cascadeIndex, float3 P, float3 N, float3 V)
{
    const float3 spacing = DDGIProbeSpacing(cascadeIndex);
    const int3 counts = DDGIProbeCounts();
    const int3 gridOffset = ddgiConstants.cascades[cascadeIndex].gridOffset.xyz;
    const float normalBias = ddgiConstants.cascades[cascadeIndex].normalBias;

    const int3 baseGridCoord = clamp(int3(floor(P / spacing)), gridOffset, gridOffset + counts - 2);
    const float3 baseProbePosition = float3(baseGridCoord) * spacing;

    const float3 alpha = saturate((P - baseProbePosition) / spacing);

    float3 totalIrradiance = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < 8; i++)
    {
        int3 offset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);

        int3 gridCoord = baseGridCoord + offset;
        uint probeIndex = DDGIProbeIndex(cascadeIndex, DDGIWrapCoord(gridCoord));

        float3 probePosition = float3(gridCoord) * spacing;
        float3 probeToPoint = P - probePosition + (N + 3.0 * V) * normalBias;
        float3 dir = normalize(-probeToPoint);

        float3 trilinear = lerp(float3(1.0, 1.0, 1.0) - alpha, alpha, float3(offset));
        float weight = 1.0;

        /* Backface test */

        float3 trueDirectionToProbe = normalize(probePosition - P);
        weight *= HYP_FMATH_SQR(max(0.0001, (dot(trueDirectionToProbe, N) + 1.0) * 0.5)) + 0.2;

        /* Visibility test */
        float2 depthTexcoord = TextureCoordFromDirection(-dir, probeIndex, ddgiConstants.imageDimensions.zw, DDGI_PROBE_SIDE_LENGTH_DEPTH);
        float distanceToProbe = length(probeToPoint);

        float2 depthSample = SAMPLE_TEXTURE_2D_LOD(gbuffer_sampler, probe_depth, depthTexcoord, 0.0).rg;

        float mean = depthSample.x;
        float variance = abs(HYP_FMATH_SQR(mean) - depthSample.y);

        float chebyshev = variance / (variance + HYP_FMATH_SQR(max(distanceToProbe - mean, 0.0)));
        chebyshev = max(HYP_FMATH_CUBE(chebyshev), 0.0);
        weight *= (distanceToProbe <= mean) ? 1.0 : chebyshev;
        weight = max(0.0001, weight);

        float2 irradianceTexcoord = TextureCoordFromDirection(normalize(N), probeIndex, ddgiConstants.imageDimensions.xy, DDGI_PROBE_SIDE_LENGTH_IRRADIANCE);
        float3 irradiance = SAMPLE_TEXTURE_2D_LOD(gbuffer_sampler, probe_irradiance, irradianceTexcoord, 0.0).rgb;

        const float crushThreshold = 0.2;
        if (weight < crushThreshold)
        {
            weight *= weight * weight * (1.0 / HYP_FMATH_SQR(crushThreshold));
        }

        // trilinear
        weight *= trilinear.x * trilinear.y * trilinear.z;

        totalIrradiance += irradiance * weight;
        totalWeight += weight;
    }

    float3 netIrradiance = totalIrradiance / max(totalWeight, 0.001);

    return float4(netIrradiance, 1.0);
}

/* Samples the finest cascade that contains P, cross fading into the next one across its outer band.
   Alpha is zero outside of the coarsest cascade so the caller can fall back to other sources of indirect light. */
float4 DDGISampleIrradiance(float3 P, float3 N, float3 V)
{
    for (uint cascadeIndex = 0; cascadeIndex < ddgiConstants.numCascades; cascadeIndex++)
    {
        const float weight = DDGICascadeWeight(cascadeIndex, P);

        if (weight <= 0.0)
        {
            continue;
        }

        const float4 irradiance = DDGISampleCascadeIrradiance(cascadeIndex, P, N, V);

        if (cascadeIndex + 1 >= ddgiConstants.numCascades)
        {
            return float4(irradiance.rgb, weight);
        }

        if (weight >= 1.0)
        {
            return irradiance;
        }

        return lerp(DDGISampleCascadeIrradiance(cascadeIndex + 1, P, N, V), irradiance, weight);
    }

    return float4(0.0, 0.0, 0.0, 0.0);
}

#endif
