#ifndef HYP_CLOUD_MARCH
#define HYP_CLOUD_MARCH

// Marching and lighting through the cloud layer, shared by the sky trace and the sky probe so both see the same clouds.
// Include after Scene.hlsli, EnvProbes.hlsli, Atmosphere.hlsli and CloudDensity.hlsli

// world units are meters; the planet is centered straight below the ray origin
static const float CloudMaxTraceDistance = 30000.0;

static const float LightMarchFirstStep = 60.0;
static const float LightMarchStepGrowth = 1.8;

// coarse steps skip empty space; this many empty fine samples in a row drops back to coarse
static const uint EmptyFineSamplesBeforeCoarse = 6;

float HenyeyGreenstein(float cosTheta, float g)
{
    const float gSquared = g * g;
    const float denominator = pow(max(1.0 + gSquared - 2.0 * g * cosTheta, 1e-4), 1.5);

    return (1.0 - gSquared) / (4.0 * HYP_FMATH_PI * denominator);
}

// strong forward lobe for the silver lining, weak back lobe so clouds facing away from the sun aren't flat
float CloudPhase(float cosTheta, float eccentricityScale)
{
    return lerp(HenyeyGreenstein(cosTheta, 0.8 * eccentricityScale), HenyeyGreenstein(cosTheta, -0.2 * eccentricityScale), 0.25);
}

struct CloudLayer
{
    float3 planetCenter;
    float innerRadius;
    float outerRadius;
};

CloudLayer GetCloudLayer(CloudVolumeParams params, float3 rayOrigin)
{
    CloudLayer layer;
    layer.planetCenter = float3(rayOrigin.x, -PLANET_RADIUS, rayOrigin.z);
    layer.innerRadius = PLANET_RADIUS + params.baseAltitude;
    layer.outerRadius = layer.innerRadius + params.layerThickness;

    return layer;
}

float GetHeightFraction(CloudLayer layer, float3 position)
{
    return (length(position - layer.planetCenter) - layer.innerRadius) / (layer.outerRadius - layer.innerRadius);
}

float MarchOpticalDepthToSun(CloudLayer layer, float3 position, float3 directionToSun, uint lightSteps)
{
    float opticalDepth = 0.0;
    float stepLength = LightMarchFirstStep;
    float distanceAlongRay = 0.0;

    for (uint i = 0; i < lightSteps; i++)
    {
        const float3 samplePosition = position + directionToSun * (distanceAlongRay + stepLength * 0.5);
        const float heightFraction = GetHeightFraction(layer, samplePosition);

        if (heightFraction > 1.0)
        {
            break;
        }

        opticalDepth += SampleCloudDensity(samplePosition, heightFraction, true) * CloudExtinction * stepLength;

        distanceAlongRay += stepLength;
        stepLength *= LightMarchStepGrowth;
    }

    return opticalDepth;
}

// Nearest and farthest distances along the ray inside the cloud layer; x >= y means the ray misses it
float2 GetCloudLayerRayRange(CloudLayer layer, float3 rayOrigin, float3 rayDirection)
{
    const float3 originFromCenter = rayOrigin - layer.planetCenter;
    const float originRadius = length(originFromCenter);

    // ground only camera - rays that hit the planet never reach the clouds
    if (RaySphereIntersection(originFromCenter, rayDirection, PLANET_RADIUS).x > 0.0)
    {
        return float2(1.0, 0.0);
    }

    const float2 innerHit = RaySphereIntersection(originFromCenter, rayDirection, layer.innerRadius);
    const float2 outerHit = RaySphereIntersection(originFromCenter, rayDirection, layer.outerRadius);

    if (outerHit.y <= 0.0)
    {
        return float2(1.0, 0.0);
    }

    const float rangeStart = originRadius < layer.innerRadius
        ? innerHit.y
        : (originRadius < layer.outerRadius ? 0.0 : max(outerHit.x, 0.0));

    const float rangeEnd = (originRadius > layer.innerRadius && innerHit.x > 0.0)
        ? innerHit.x
        : outerHit.y;

    return float2(max(rangeStart, 0.0), min(rangeEnd, rangeStart + CloudMaxTraceDistance));
}

struct CloudLighting
{
    float3 directionToSun;
    float3 sunRadiance;
    float3 skyAmbient;
    float3 groundAmbient;
};

CloudLighting GetCloudLighting(Light sun, bool hasSun, EnvProbe skyProbe, CloudVolumeParams params)
{
    CloudLighting lighting;
    lighting.directionToSun = hasSun ? normalize(sun.position_intensity.xyz) : float3(0.0, 1.0, 0.0);

    // one atmosphere lookup for the whole layer - sun color barely changes across it
    lighting.sunRadiance = hasSun
        ? sun.color.rgb * sun.position_intensity.w * GetSunTransmittance(params.baseAltitude + params.layerThickness * 0.5, lighting.directionToSun)
        : (float3)0.0;

    const bool hasSkyProbe = skyProbe.textureIndices != ~0u;

    float shUpBands[9];
    float shDownBands[9];
    ProjectSHBands(float3(0.0, 1.0, 0.0), shUpBands);
    ProjectSHBands(float3(0.0, -1.0, 0.0), shDownBands);

    lighting.skyAmbient = hasSkyProbe ? max(EnvProbeSH(skyProbe, shUpBands), (float3)0.0) : (float3)0.1;
    lighting.groundAmbient = hasSkyProbe ? max(EnvProbeSH(skyProbe, shDownBands), (float3)0.0) : (float3)0.03;

    return lighting;
}

struct CloudMarchResult
{
    // not yet hazed; see ApplyCloudHaze
    float3 scatteredLight;
    float transmittance;

    // transmittance weighted distance to the cloud, or the far end of the ray range when it hit nothing
    float distance;
};

CloudMarchResult MarchCloudLayer(
    CloudLayer layer,
    CloudLighting lighting,
    float3 rayOrigin,
    float3 rayDirection,
    float2 rayRange,
    uint traceSteps,
    uint lightSteps,
    float jitter)
{
    const float cosTheta = dot(rayDirection, lighting.directionToSun);

    // horizon rays cross far more of the layer, so they get more steps
    const float horizonFactor = 1.0 - saturate(abs(rayDirection.y) * 4.0);
    const uint numFineSteps = uint(lerp(float(traceSteps), float(traceSteps) * 2.0, horizonFactor));

    const float fineStepLength = (rayRange.y - rayRange.x) / float(max(numFineSteps, 1u));
    const float coarseStepLength = fineStepLength * 2.0;

    float distanceAlongRay = rayRange.x + fineStepLength * jitter;

    float transmittance = 1.0;
    float3 scatteredLight = (float3)0.0;

    float weightedDistanceSum = 0.0;
    float distanceWeightSum = 0.0;

    bool isFineMarching = false;
    uint emptyFineSamples = 0;

    const uint maxIterations = numFineSteps * 2u;

    for (uint i = 0; i < maxIterations; i++)
    {
        if (distanceAlongRay >= rayRange.y || transmittance < 0.01)
        {
            break;
        }

        const float3 samplePosition = rayOrigin + rayDirection * distanceAlongRay;
        const float heightFraction = GetHeightFraction(layer, samplePosition);

        if (!isFineMarching)
        {
            if (SampleCloudDensity(samplePosition, heightFraction, true) > 0.0)
            {
                // step back so the fine march starts before the cloud edge
                isFineMarching = true;
                emptyFineSamples = 0;
                distanceAlongRay = max(rayRange.x, distanceAlongRay - coarseStepLength);

                continue;
            }

            distanceAlongRay += coarseStepLength;

            continue;
        }

        const float density = SampleCloudDensity(samplePosition, heightFraction, false);

        if (density > 0.0)
        {
            emptyFineSamples = 0;

            const float extinction = max(density * CloudExtinction, 1e-6);
            const float opticalDepthToSun = MarchOpticalDepthToSun(layer, samplePosition, lighting.directionToSun, lightSteps);

            // multiple scattering approximation (Wrenninge 2013): each octave scatters more, attenuates less and is less directional
            float sunScattering = 0.0;
            float octaveScattering = 1.0;
            float octaveAttenuation = 1.0;
            float octaveEccentricity = 1.0;

            for (uint octave = 0; octave < 3; octave++)
            {
                sunScattering += octaveScattering * CloudPhase(cosTheta, octaveEccentricity) * exp(-opticalDepthToSun * octaveAttenuation);

                octaveScattering *= 0.5;
                octaveAttenuation *= 0.5;
                octaveEccentricity *= 0.5;
            }

            const float3 ambient = lerp(lighting.groundAmbient, lighting.skyAmbient, saturate(heightFraction));
            const float3 sourceRadiance = (lighting.sunRadiance * sunScattering + ambient) * extinction;

            const float stepTransmittance = exp(-extinction * fineStepLength);

            // energy conserving integration over the step (Hillaire 2016)
            scatteredLight += transmittance * (sourceRadiance - sourceRadiance * stepTransmittance) / extinction;

            const float absorbed = transmittance * (1.0 - stepTransmittance);
            weightedDistanceSum += distanceAlongRay * absorbed;
            distanceWeightSum += absorbed;

            transmittance *= stepTransmittance;
        }
        else if (++emptyFineSamples >= EmptyFineSamplesBeforeCoarse)
        {
            isFineMarching = false;
        }

        distanceAlongRay += fineStepLength;
    }

    CloudMarchResult result;
    result.scatteredLight = scatteredLight;
    result.transmittance = transmittance;
    result.distance = distanceWeightSum > 0.0 ? weightedDistanceSum / distanceWeightSum : rayRange.y;

    return result;
}

// Far clouds fade into the sky behind them, and out entirely before the weather map and trace distance run out.
// skyColor is the cloud-free sky in the ray's direction
float3 ApplyCloudHaze(CloudMarchResult march, float rayRangeStart, float hazeDistance, float3 skyColor)
{
    const float distanceFade = saturate((CloudMaxTraceDistance - (march.distance - rayRangeStart)) / (CloudMaxTraceDistance * 0.25));
    const float hazeFactor = exp(-march.distance / hazeDistance) * distanceFade;

    return march.scatteredLight * hazeFactor + (1.0 - march.transmittance) * (1.0 - hazeFactor) * skyColor;
}

#endif // HYP_CLOUD_MARCH
