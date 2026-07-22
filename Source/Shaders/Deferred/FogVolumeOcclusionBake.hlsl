#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/BRDF.hlsli"

struct PointLightBakeData
{
    float4 positionRadius; // xyz = world position, w = falloff radius
    float4 colorIntensity; // rgb = color, w = intensity
};

DECLARE_SRV(FogVolumeOcclusionBake, OcclusionSDF) Texture3D<float> OcclusionSDF;
DECLARE_SAMPLER(FogVolumeOcclusionBake, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SRV(FogVolumeOcclusionBake, PointLights) StructuredBuffer<PointLightBakeData> PointLights;
DECLARE_UAV(FogVolumeOcclusionBake, OutputBuffer) RWStructuredBuffer<float4> OutputBuffer;

DECLARE_BUFFER(FogVolumeOcclusionBake, CBuffer) cbuffer CBuffer
{
    uint4 dimensionsAndNumLights; // xyz = volume dimensions, w = number of point lights
    float4 aabbMin;
    float4 aabbMax;
    float4 samplesAndRadiusScale; // x = numSamples, y = source radius scale, zw = unused
};

float3 FibonacciSpherePoint(uint index, uint numSamples)
{
    static const float s_goldenAngle = 2.39996323; // pi * (3 - sqrt(5))

    float t = (float(index) + 0.5) / float(numSamples);
    float z = 1.0 - 2.0 * t;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float theta = s_goldenAngle * float(index);

    return float3(r * cos(theta), r * sin(theta), z);
}

static const float OutOfBoundsSdfDistance = 10000.0;

float SampleOcclusionSdf(float3 positionWS)
{
    float3 uvw = (positionWS - aabbMin.xyz) / (aabbMax.xyz - aabbMin.xyz);

    if (any(uvw < 0.0) || any(uvw > 1.0))
    {
        return OutOfBoundsSdfDistance;
    }

    return SAMPLE_TEXTURE_3D_LOD(SamplerLinear, OcclusionSDF, uvw, 0).r;
}

bool SphereTraceOccluded(float3 origin, float3 target, float targetBias)
{
    float3 dir = target - origin;
    float targetDist = length(dir);

    if (targetDist <= 0.0001)
    {
        return false;
    }

    dir /= targetDist;

    const float hitEpsilon = 0.01;
    const int maxSteps = 64;

    float t = hitEpsilon * 2.0; // small bias to avoid self-occlusion at the origin

    for (int i = 0; i < maxSteps; i++)
    {
        if (t >= targetDist - targetBias)
        {
            return false; // reached the target without hitting anything -> visible
        }

        float3 p = origin + dir * t;
        float sdf = SampleOcclusionSdf(p);

        if (sdf < hitEpsilon)
        {
            return true; // occluded
        }

        t += max(sdf, hitEpsilon);
    }

    return true;
}

[numthreads(4, 4, 4)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint3 dims = dimensionsAndNumLights.xyz;
    const uint3 texelCoord = dispatchThreadID;

    if (any(texelCoord >= dims))
    {
        return;
    }

    const float3 extentWS = aabbMax.xyz - aabbMin.xyz;
    const float3 texelHalfSizeWS = extentWS * (0.5 / float3(dims));
    const float3 posWS = aabbMin.xyz + (extentWS * (float3(texelCoord) / float3(dims))) + texelHalfSizeWS;

    const uint numLights = dimensionsAndNumLights.w;
    const uint numSamples = uint(samplesAndRadiusScale.x);
    const float sourceRadiusScale = samplesAndRadiusScale.y;

    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    float accumulatedWeight = 0.0;
    float visibility = 0.0;

    for (uint lightIndex = 0; lightIndex < numLights; lightIndex++)
    {
        PointLightBakeData light = PointLights[lightIndex];

        const float3 lightPosition = light.positionRadius.xyz;
        const float radius = light.positionRadius.w;

        const float3 toLight = lightPosition - posWS;
        const float dist = length(toLight);

        if (dist >= radius)
        {
            continue;
        }

        const float attenuation = GetSquareFalloffAttenuation(posWS, lightPosition, radius);

        const float sourceRadius = radius * sourceRadiusScale;

        const float targetBias = max(sourceRadius * 2.0, 0.05);

        uint numUnoccluded = 0;

        for (uint sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
        {
            const float3 samplePosition = lightPosition + FibonacciSpherePoint(sampleIndex, numSamples) * sourceRadius;

            if (!SphereTraceOccluded(posWS, samplePosition, targetBias))
            {
                numUnoccluded++;
            }
        }

        const float shadow = float(numUnoccluded) / float(numSamples);

        const float3 lightColor = light.colorIntensity.rgb * light.colorIntensity.w;

        accumulatedColor += lightColor * attenuation * shadow;
        accumulatedWeight += attenuation;
        visibility += attenuation * shadow;
    }

    if (accumulatedWeight > 0.0001)
    {
        visibility /= accumulatedWeight;
    }
    else
    {
        visibility = 1.0;
    }

    const uint flatIndex = texelCoord.z * (dims.x * dims.y) + texelCoord.y * dims.x + texelCoord.x;
    OutputBuffer[flatIndex] = float4(accumulatedColor, visibility);
}
