#ifndef GLIMMER_LOCAL_LIGHTS_HLSLI
#define GLIMMER_LOCAL_LIGHTS_HLSLI

#define GLIMMER_MAX_LOCAL_LIGHTS 32

#define GLIMMER_LOCAL_LIGHT_NEAR 0.25
#define GLIMMER_LOCAL_LIGHT_SHADOW_BIAS 0.05

struct GlimmerLocalLight
{
    float4 positionRadius;
    /// color premultiplied by intensity.
    float4 color;
    
    /// w = 1 if it is a spot light
    float4 spotDirection;
    
    /// x = cos of the outer angle
    /// y = cos of the inner angle
    float4 spotAngles;
};

struct GlimmerLocalLights
{
    uint4 info; // x = number of lights
    GlimmerLocalLight lights[GLIMMER_MAX_LOCAL_LIGHTS];
};

#endif

// GLIMMER_LOCAL_LIGHT_TRANSMITTANCE(origin, direction, tMax) is the including shader's shadow test: how much of the light gets through
#if defined(GLIMMER_LOCAL_LIGHT_TRANSMITTANCE) && !defined(GLIMMER_LOCAL_LIGHTS_SAMPLING_HLSLI)
#define GLIMMER_LOCAL_LIGHTS_SAMPLING_HLSLI

float3 GlimmerLocalLightsAt(GlimmerLocalLights localLights, float3 P, float3 N)
{
    float3 received = (float3)0.0;

    const float3 origin = P + N * GLIMMER_LOCAL_LIGHT_SHADOW_BIAS;

    for (uint lightIndex = 0; lightIndex < min(localLights.info.x, uint(GLIMMER_MAX_LOCAL_LIGHTS)); lightIndex++)
    {
        const GlimmerLocalLight light = localLights.lights[lightIndex];

        const float radius = light.positionRadius.w;
        const float3 toLight = light.positionRadius.xyz - P;
        const float distanceSq = dot(toLight, toLight);

        if (distanceSq >= radius * radius)
        {
            continue;
        }

        const float3 L = toLight * rsqrt(max(distanceSq, 1e-8));
        const float NdotL = dot(N, L);

        if (NdotL <= 0.0)
        {
            continue;
        }

        const float distanceOverRadiusSq = distanceSq / (radius * radius);
        const float window = saturate(1.0 - distanceOverRadiusSq * distanceOverRadiusSq);

        float attenuation = window * window / max(distanceSq, GLIMMER_LOCAL_LIGHT_NEAR * GLIMMER_LOCAL_LIGHT_NEAR);

        if (light.spotDirection.w > 0.0)
        {
            const float theta = dot(-L, light.spotDirection.xyz);

            attenuation *= saturate((theta - light.spotAngles.x) / (light.spotAngles.y - light.spotAngles.x)) * step(light.spotAngles.x, theta);
        }

        if (attenuation <= 0.0)
        {
            continue;
        }

        const float3 originToLight = light.positionRadius.xyz - origin;
        const float shadowDistance = length(originToLight) - GLIMMER_LOCAL_LIGHT_NEAR;

        float transmittance = 1.0;

        if (shadowDistance > 0.0)
        {
            transmittance = GLIMMER_LOCAL_LIGHT_TRANSMITTANCE(origin, originToLight / max(shadowDistance + GLIMMER_LOCAL_LIGHT_NEAR, 1e-4), shadowDistance);
        }

        if (transmittance <= 0.0)
        {
            continue;
        }

        received += light.color.rgb * (attenuation * NdotL * transmittance * HYP_FMATH_ONE_OVER_PI);
    }

    return received;
}

#endif
