#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/BRDF.hlsli"

struct FogVolumeLightBakeData
{
    float4 positionRadiusType;  // xyz = world position, w = asfloat(f16(radius) | (type << 16) | (quantized falloff << 20))
    float4 colorIntensity;      // rgb = color, w = intensity
    float4 directionSpotAngles; // xyz = spot direction (normal); w = asfloat(f16(outerAngle) | (f16(innerAngle) << 16))
};

static const float MaxLightFalloffExponent = 8.0;

struct FogVolumeEnvProbeBakeData
{
    float4 positionRadius; // xyz = world position, w = influence radius
    float4 ambientColor;
};

DECLARE_SRV(FogVolumeOcclusionBake, OcclusionSDF) Texture3D<float> OcclusionSDF;
DECLARE_SAMPLER(FogVolumeOcclusionBake, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SRV(FogVolumeOcclusionBake, Lights) StructuredBuffer<FogVolumeLightBakeData> Lights;
DECLARE_SRV(FogVolumeOcclusionBake, EnvProbes) StructuredBuffer<FogVolumeEnvProbeBakeData> EnvProbes;
DECLARE_UAV(FogVolumeOcclusionBake, OutputBuffer) RWStructuredBuffer<float4> OutputBuffer;

DECLARE_BUFFER(FogVolumeOcclusionBake, CBuffer) cbuffer CBuffer
{
    uint4 dimensionsAndNumLights; // xyz = volume dimensions, w = number of lights
    float4 aabbMin;
    float4 aabbMax;
    float4 samplesAndRadiusScaleAndNumEnvProbes; // x = numSamples, y = source radius scale, z = numEnvProbes, w = unused
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

float GetLightAttenuation(float3 P, float3 L, float radius, float falloffExponent, float minDistance)
{
    const float3 toLight = P - L;
    const float distanceSq = max(dot(toLight, toLight), minDistance * minDistance);
    const float radiusSq = radius * radius;
    const float inverseSquare = 1.0 / max(distanceSq, 0.0001);

    const float distanceOverRadiusSq = distanceSq / radiusSq;
    const float distanceOverRadiusQuad = distanceOverRadiusSq * distanceOverRadiusSq;

    float window = saturate(1.0 - distanceOverRadiusQuad);
    window *= window;
    window = pow(window, falloffExponent);

    return inverseSquare * window;
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
    const float3 texelSizeWS = extentWS / float3(dims);
    const float3 texelHalfSizeWS = texelSizeWS * 0.5;
    const float3 posWS = aabbMin.xyz + (extentWS * (float3(texelCoord) / float3(dims))) + texelHalfSizeWS;

    const float lightMinDistance = length(texelSizeWS);

    const uint numLights = dimensionsAndNumLights.w;
    const uint numSamples = uint(samplesAndRadiusScaleAndNumEnvProbes.x);
    const float sourceRadiusScale = samplesAndRadiusScaleAndNumEnvProbes.y;
    const uint numEnvProbes = uint(samplesAndRadiusScaleAndNumEnvProbes.z);

    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    float accumulatedWeight = 0.0;
    float visibility = 0.0;

    for (uint lightIndex = 0; lightIndex < numLights; lightIndex++)
    {
        FogVolumeLightBakeData light = Lights[lightIndex];

        const uint posRadiusTypePacked = asuint(light.positionRadiusType.w);
        const float radius = f16tof32(posRadiusTypePacked & 0xFFFFu);
        const uint lightType = (posRadiusTypePacked >> 16) & 0xFu;
        const uint falloffQuantized = (posRadiusTypePacked >> 20) & 0xFFFu;
        const float falloffExponent = (float(falloffQuantized) / 4095.0) * MaxLightFalloffExponent;

        const float3 lightPosition = light.positionRadiusType.xyz;

        const float3 toLight = lightPosition - posWS;
        const float dist = length(toLight);

        if (dist >= radius)
        {
            continue;
        }

        float attenuation = GetLightAttenuation(posWS, lightPosition, radius, falloffExponent, lightMinDistance);

        if (lightType == HYP_LIGHT_TYPE_SPOT)
        {
            const uint anglesPacked = asuint(light.directionSpotAngles.w);
            const float outerAngle = f16tof32(anglesPacked & 0xFFFFu);
            const float innerAngle = f16tof32((anglesPacked >> 16) & 0xFFFFu);

            const float3 spotNormal = normalize(light.directionSpotAngles.xyz);

            const float3 worldToLight = posWS - lightPosition;
            const float3 lightDir = normalize(-worldToLight);
            const float theta = max(dot(-lightDir, spotNormal), 0.0);

            attenuation *= saturate((theta - outerAngle) / (innerAngle - outerAngle)) * step(outerAngle, theta);
        }

        if (attenuation <= 0.0001)
        {
            continue;
        }

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

    float3 ambientColor = float3(0.0, 0.0, 0.0);
    float ambientWeight = 0.0;

    for (uint probeIndex = 0; probeIndex < numEnvProbes; probeIndex++)
    {
        FogVolumeEnvProbeBakeData envProbe = EnvProbes[probeIndex];

        const float3 toProbe = envProbe.positionRadius.xyz - posWS;
        const float dist = length(toProbe);
        const float radius = envProbe.positionRadius.w;

        if (dist >= radius)
        {
            continue;
        }

        const float falloff = saturate(1.0 - (dist / max(radius, 0.0001)));
        const float weight = falloff * falloff;

        ambientColor += envProbe.ambientColor.rgb * weight;
        ambientWeight += weight;
    }

    if (ambientWeight > 0.0001)
    {
        ambientColor /= ambientWeight;
    }

    accumulatedColor += ambientColor;

    const uint flatIndex = texelCoord.z * (dims.x * dims.y) + texelCoord.y * dims.x + texelCoord.x;
    OutputBuffer[flatIndex] = float4(accumulatedColor, visibility);
}
