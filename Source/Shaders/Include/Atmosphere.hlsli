#ifndef HYP_ATMOSPHERE
#define HYP_ATMOSPHERE

// Shared by the sky (Sky/RenderSky.hlsl) and anything lit through the atmosphere (clouds), so they agree on sun color

#define PLANET_RADIUS 6371e3
#define ATMOSPHERE_RADIUS 6471e3

#define RAYLEIGH_SCATTER_COEFF float3(5.5e-6, 13.0e-6, 22.4e-6)
#define RAYLEIGH_SCATTER_HEIGHT 8e3

#define MIE_SCATTER_COEFF 21e-6
#define MIE_SCATTER_HEIGHT 1.2e3
#define MIE_SCATTER_DIRECTION 0.758

float2 RaySphereIntersection(float3 r0, float3 rd, float sr)
{
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b * b) - 4.0 * a * c;

    if (d < 0.0)
    {
        return float2(1e5, -1e5);
    }

    return float2(
        (-b - sqrt(d)) / (2.0 * a),
        (-b + sqrt(d)) / (2.0 * a));
}

// Fraction of sunlight left after passing through the atmosphere to a point at altitude (meters above sea level)
float3 GetSunTransmittance(float altitude, float3 directionToSun)
{
    static const uint NumSteps = 8;

    const float3 origin = float3(0.0, PLANET_RADIUS + max(altitude, 1.0), 0.0);

    // sun below the horizon from this point
    if (RaySphereIntersection(origin, directionToSun, PLANET_RADIUS).x > 0.0)
    {
        return (float3)0.0;
    }

    const float pathLength = RaySphereIntersection(origin, directionToSun, ATMOSPHERE_RADIUS).y;
    const float stepLength = pathLength / float(NumSteps);

    float rayleighDepth = 0.0;
    float mieDepth = 0.0;

    for (uint i = 0; i < NumSteps; i++)
    {
        const float3 samplePosition = origin + directionToSun * ((float(i) + 0.5) * stepLength);
        const float sampleAltitude = length(samplePosition) - PLANET_RADIUS;

        rayleighDepth += exp(-sampleAltitude / RAYLEIGH_SCATTER_HEIGHT) * stepLength;
        mieDepth += exp(-sampleAltitude / MIE_SCATTER_HEIGHT) * stepLength;
    }

    return exp(-(MIE_SCATTER_COEFF * mieDepth + RAYLEIGH_SCATTER_COEFF * rayleighDepth));
}

#endif // HYP_ATMOSPHERE
