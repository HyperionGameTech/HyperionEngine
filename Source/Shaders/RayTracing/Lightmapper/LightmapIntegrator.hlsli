#ifndef HYP_LIGHTMAP_INTEGRATOR
#define HYP_LIGHTMAP_INTEGRATOR

// Shared by the hardware ray tracing and compute lightmap path tracers

struct LightmapRay
{
    float3 origin;
    float3 direction;
};

#define RAY_OFFSET 0.01

#define VSM_DEPTH_BIAS_CONSTANT 0.2
#define VSM_DEPTH_BIAS_SLOPE_SCALE 0.02
#define VSM_DEPTH_BIAS_SLOPE_MAX 8.0

#ifdef MODE_LIGHTMAP
#define NUM_BOUNCES 4
#define NUM_SAMPLES 256
#define ENVIRONMENT_INTENSITY 1.0
#elif defined(MODE_RADIANCE) || defined(MODE_IRRADIANCE)
#define NUM_BOUNCES 8
#define NUM_SAMPLES 32
#define ENVIRONMENT_INTENSITY 1.0
#elif defined(MODE_BENT_NORMALS)
#define NUM_BOUNCES 1
#define NUM_SAMPLES 32
#define ENVIRONMENT_INTENSITY 1.0
#else
#define NUM_BOUNCES 1
#define NUM_SAMPLES 1
#define ENVIRONMENT_INTENSITY 1.0
#endif

#if defined(MODE_RADIANCE) || defined(MODE_IRRADIANCE) || defined(MODE_LIGHTMAP)

#define MAX_SAMPLE_LUMINANCE 8.0

float3 ClampLuminance(in float3 c, float max_lum)
{
    float lum = Luminance(c);
    if (lum > max_lum)
    {
        return c * (max_lum / max(lum, 1e-6));
    }
    return c;
}
#endif

float3 DebugTest_Albedo(in float3 position, in float3 normal, inout RayPayload payload)
{
    float4 saved_throughput = payload.throughput;
    float4 saved_emissive = payload.emissive;
    float saved_distance = payload.distance;
    float3 saved_normal = payload.normal;
    float3 saved_bary = payload.barycentric_coords;

    float3 origin = position + normal * 0.001;
    float3 direction = -normal;

    payload.distance = -1.0;
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.normal = float3(0.0, 0.0, 0.0);

    TraceScene(origin, direction, 0.0, 1.0, payload);

    float3 albedo = float3(0.0, 0.0, 0.0);
    if (payload.distance >= 0.0)
    {
        albedo = payload.throughput.rgb;
    }

    payload.throughput = saved_throughput;
    payload.emissive = saved_emissive;
    payload.distance = saved_distance;
    payload.normal = saved_normal;
    payload.barycentric_coords = saved_bary;

    return albedo;
}

#if defined(MODE_IRRADIANCE) || defined(MODE_RADIANCE) || defined(MODE_LIGHTMAP)

float3 SampleDirectLighting(in float3 hitPos, in float3 N)
{
    float3 result = float3(0.0, 0.0, 0.0);

    for (uint light_index = 0; light_index < min(rayTracingConstants.numBoundLights, 16); light_index++)
    {
        const Light light = lights[light_index];
        const float3 light_color = light.color.rgb * light.position_intensity.w;

        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL)
        {
            const float3 L = normalize(light.position_intensity.xyz);
            const float NdotL = max(dot(N, L), 0.0);

            if (NdotL <= 0.0)
            {
                continue;
            }

            const float shadow = 1.0 - CheckInShadow(hitPos, N, L);
            result += light_color * NdotL * shadow;
        }
        else if (light.type == HYP_LIGHT_TYPE_POINT || light.type == HYP_LIGHT_TYPE_SPOT)
        {
            const float3 toLight = light.position_intensity.xyz - hitPos;
            const float d2 = max(dot(toLight, toLight), 1e-6);
            const float d = sqrt(d2);
            const float3 L = toLight / d;

            const float NdotL = max(dot(N, L), 0.0);

            if (NdotL <= 0.0)
            {
                continue;
            }

            const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
            const float radius = radiusFalloff.x;

            float attenuation = GetSquareFalloffAttenuation(hitPos, light.position_intensity.xyz, radius);

            if (light.type == HYP_LIGHT_TYPE_SPOT)
            {
                const float theta = max(dot(-L, normalize(light.normal.xyz)), 0.0);
                const float2 spot_angles = light.area_size.xy;

                attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);
            }

            if (attenuation <= 0.0)
            {
                continue;
            }

            const float shadow = 1.0 - CheckInShadow(hitPos, N, L, max(0.0, d - RAY_OFFSET));
            result += light_color * NdotL * attenuation * shadow;
        }
        // TODO: area lights
    }

    return result;
}
#endif

float4 SampleEnvironment(float3 origin, float3 direction)
{
    float4 environmentRadiance = (float4) 0.0;

    for (uint envProbeIdx = 0; envProbeIdx < rayTracingConstants.numBoundEnvProbes && environmentRadiance.a < 1.0; envProbeIdx++)
    {
        EnvProbe currentEnvProbe = envProbes[envProbeIdx];

        const uint textureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(currentEnvProbe);

        const uint probeType = GET_ENV_PROBE_TYPE(currentEnvProbe);
        const bool isSky = (probeType == EPT_SKY);

        if (!isSky)
        {
            continue;
        }

        const float4 aabbMin = currentEnvProbe.aabb_min;
        const float4 aabbMax = currentEnvProbe.aabb_max;

        const float4 worldPosition = currentEnvProbe.world_position;
        const float3 worldPosition3 = worldPosition.xyz;
        const float diffuseStrength = worldPosition.w;

        const float weight = (isSky ? 1.0 : CalculateEnvProbeWeight(origin, aabbMin.xyz, aabbMax.xyz, 0.005)) * diffuseStrength * (1.0 - environmentRadiance.a);

        const float4 env = EnvProbeSample(sampler_linear, envProbesColorTexture, textureIndex, direction, 0.0);
        environmentRadiance += env * weight;
    }

    return environmentRadiance;
}

float4 IntegrateLightmapRay(uint ray_index)
{
    LightmapRay ray;
    ray.origin = ray_data[ray_index * 2].xyz;
    ray.direction = ray_data[ray_index * 2 + 1].xyz;

    const float tmin = RAY_OFFSET;
    const float tmax = rayTracingConstants.maxDistance;

    const float3 firstRayDirection = normalize(ray.direction);

    float3 tangent;
    float3 bitangent;

    uint ray_seed = InitRandomSeed(((rayTracingConstants.ray_offset + ray_index) * 2), ((rayTracingConstants.ray_offset + ray_index) * 2) + 1);

    RayPayload payload = (RayPayload)0;

#ifdef MODE_LIGHTMAP
    float4 accumRadiance = float4(0.0, 0.0, 0.0, 0.0);

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = SampleCosineDir(rnd, firstRayDirection);
        direction = normalize(direction);

        float3 origin = ray.origin + firstRayDirection * RAY_OFFSET;

        float3 radiance = float3(0.0, 0.0, 0.0);
        float3 beta = float3(1.0, 1.0, 1.0);

        for (int bounceIndex = 0; bounceIndex < NUM_BOUNCES; ++bounceIndex)
        {
            // perform scene trace using global payload
            payload.distance = -1.0;
            payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
            payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
            payload.normal = float3(0.0, 0.0, 0.0);

            TraceScene(origin, direction, tmin, tmax, payload);

            if (payload.distance < 0.0)
            {
                float4 environmentRadiance = (float4)0.0;

                for (uint envProbeIdx = 0; envProbeIdx < min(rayTracingConstants.numBoundEnvProbes, 16) && environmentRadiance.a < 1.0; envProbeIdx++)
                {
                    // EnvProbe currentEnvProbe = envProbes[envProbeIdx];

                    // const uint probeType = GET_ENV_PROBE_TYPE(currentEnvProbe);
                    // const bool isSky = (probeType == EPT_SKY);

                    // const float4 aabbMin = currentEnvProbe.aabb_min;
                    // const float4 aabbMax = currentEnvProbe.aabb_max;

                    // const float4 worldPosition = currentEnvProbe.world_position;
                    // const float3 worldPosition3 = worldPosition.xyz;
                    // const float diffuseStrength = worldPosition.w;

                    // const float weight = (isSky ? 1.0 : CalculateEnvProbeWeight(origin, aabbMin.xyz, aabbMax.xyz)) * diffuseStrength * (1.0 - environmentRadiance.a);

                    // const float4 env = EnvProbeSH(currentEnvProbe, direction);
                    // environmentRadiance += env * weight;

                    const uint probeType = GET_ENV_PROBE_TYPE(envProbes[envProbeIdx]);
                    const bool isSky = (probeType == EPT_SKY);

                    if (!isSky)
                    {
                        continue;
                    }

                    const uint envProbeTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(envProbes[envProbeIdx]);

                    if (envProbeTextureIndex != INVALID_ENV_PROBE_TEXTURE)
                    {
                        float4 env = EnvProbeSample(sampler_linear, envProbesColorTexture, envProbeTextureIndex, direction, 0.0);
                        env *= (1.0 - environmentRadiance.a);
                        environmentRadiance += env * ENVIRONMENT_INTENSITY;
                    }
                }

                radiance += beta * environmentRadiance.rgb;

                break;
            }

            float3 albedo = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            float metalness = clamp(payload.throughput.a, 0.0, 1.0);

            float3 hitPos = origin + direction * payload.distance;
            float3 N = normalize(payload.normal);

            // emissive
            if (any(payload.emissive.rgb > float3(0.0, 0.0, 0.0)))
            {
                // Clamp the contribution after multiplying by beta, since beta can grow
                // large from Russian roulette compensation (division by a low survival
                // probability) - clamping the light value alone doesn't bound that.
                radiance += ClampLuminance(beta * payload.emissive.rgb, MAX_SAMPLE_LUMINANCE);
            }

            float3 diffuseColor = albedo * (1.0 - metalness);

            radiance += ClampLuminance(beta * diffuseColor * HYP_FMATH_ONE_OVER_PI * SampleDirectLighting(hitPos, N), MAX_SAMPLE_LUMINANCE);

            beta *= diffuseColor;

            if (bounceIndex >= 3)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 0.99);
                if (RandomFloat(ray_seed) > p)
                {
                    break;
                }
                beta /= float3(p, p, p);
            }

            rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

            direction = normalize(SampleCosineDir(rnd, N));
            origin = hitPos + N * RAY_OFFSET;
        } // end bounces

        accumRadiance.rgb += radiance;
    } // end samples

    float4 finalColor = float4(accumRadiance.rgb / float(NUM_SAMPLES), 1.0);
#elif defined(MODE_RADIANCE)
    // direct shading

    const float3 N = firstRayDirection;

    float3 radiance = float3(0.0, 0.0, 0.0);

    for (uint light_index = 0; light_index < min(rayTracingConstants.numBoundLights, 16); light_index++)
    {
        if (lights[light_index].type == HYP_LIGHT_TYPE_DIRECTIONAL)
        {
            float3 light_direction = normalize(lights[light_index].position_intensity.xyz);
            float3 L = light_direction;

            // shadow check
            float shadow = 1.0 - CheckInShadow(ray.origin, N, L);

            float NdotL = max(dot(N, L), 0.0);
            radiance += lights[light_index].color.rgb * lights[light_index].position_intensity.w * NdotL * shadow;
        }
        else if (lights[light_index].type == HYP_LIGHT_TYPE_POINT)
        {
            float3 L = normalize(lights[light_index].position_intensity.xyz - ray.origin);
            float d = length(lights[light_index].position_intensity.xyz - ray.origin);
            float attenuation = 1.0 / (d * d);

            float NdotL = max(dot(N, L), 0.0);
            radiance += lights[light_index].color.rgb * lights[light_index].position_intensity.w * NdotL * attenuation;
        }
        else
        {
            /// ... TODO
        }
    }

    float4 finalColor = float4(radiance, 1.0);
#elif defined(MODE_RADIANCE) || defined(MODE_IRRADIANCE)
    // path traced diffuse-only light.
    float4 accumRadiance = (float4)0.0;

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd0 = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));

        float3 direction = firstRayDirection;
        float3 origin = ray.origin + firstRayDirection * RAY_OFFSET;

        float4 Li = (float4)0.0;
        float3 beta = (float3)1.0;

        bool sampleIsMiss = true;

        for (int bounceIndex = 0; bounceIndex < NUM_BOUNCES; ++bounceIndex)
        {
            // prepare payload and trace
            payload.distance = -1.0;
            payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
            payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
            payload.normal = float3(0.0, 0.0, 0.0);

            TraceScene(origin, direction, tmin, tmax, payload);

            if (payload.distance < 0.0)
            {
#ifdef MODE_RADIANCE
                // sample environment if miss but only for MODE_RADIANCE
                Li += float4(beta * SampleEnvironment(origin, direction).rgb, 1.0);
#endif

                break;
            }

            // hit something, so this sample is not a miss.
            // we can use environment probes for indirect lighting,
            // but bounces that hit e.g the sky should be left with alpha = 0 so that they can be blended with other probes.
            sampleIsMiss = false;

            // hit data
            float3 hitPos = origin + direction * payload.distance;
            float3 N = normalize(payload.normal);
            float3 baseColor = clamp(payload.throughput.rgb, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            float metalness = clamp(payload.throughput.a, 0.0, 1.0);

            // emissive contribution
            if (any(payload.emissive.rgb > float3(0.0, 0.0, 0.0)))
            {
                float3 emissiveContribution = beta * payload.emissive.rgb;

                if (bounceIndex > 0)
                {
                    emissiveContribution = ClampLuminance(emissiveContribution, MAX_SAMPLE_LUMINANCE);
                }

                Li += float4(emissiveContribution, 1.0);
            }

            float3 diffuseColor = baseColor * (1.0 - metalness);

            Li += float4(ClampLuminance(beta * diffuseColor * HYP_FMATH_ONE_OVER_PI * SampleDirectLighting(hitPos, N), MAX_SAMPLE_LUMINANCE), 1.0);

            // Russian roulette
            if (bounceIndex >= 3)
            {
                float p = clamp(max(max(beta.r, beta.g), beta.b), 0.05, 0.99);
                if (RandomFloat(ray_seed) > p)
                {
                    break;
                }
                beta /= float3(p, p, p);
            }

            float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
            float3 wi;

            wi = normalize(SampleCosineDir(rnd, N));
            beta *= diffuseColor;

            origin = hitPos + N * RAY_OFFSET;
            direction = wi;
        }

        // if the ray never hit anything, set alpha to 0 so that probes can blend between each other. If it hit something, set alpha to 1 so that the result is not blended with other probes.
        Li.a = sampleIsMiss ? 0.0 : 1.0;

        accumRadiance += Li;
    }

    float4 finalColor = accumRadiance / float(NUM_SAMPLES);

#elif defined(MODE_MOMENTS)
    payload.distance = -1.0;
    payload.throughput = float4(1.0, 1.0, 1.0, 1.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.normal = float3(0.0, 0.0, 0.0);

    TraceScene(ray.origin + ray.direction * RAY_OFFSET, ray.direction, RAY_OFFSET, rayTracingConstants.maxDistance, payload);

    float4 finalColor;

    if (payload.distance > 0.0)
    {
        const float3 hitNormal = normalize(payload.normal);
        const float cosTheta = saturate(-dot(hitNormal, ray.direction));
        const float slope = sqrt(saturate(1.0 - cosTheta * cosTheta)) / max(cosTheta, 1e-3);
        const float bias = VSM_DEPTH_BIAS_CONSTANT + VSM_DEPTH_BIAS_SLOPE_SCALE * min(slope, VSM_DEPTH_BIAS_SLOPE_MAX);

        float dist = payload.distance + bias;
        finalColor = float4(dist, dist * dist, 0.0, 1.0);
    }
    else
    {
        const float missDistance = rayTracingConstants.maxDistance;
        finalColor = float4(missDistance, missDistance * missDistance, 0.0, 1.0);
    }
#elif defined(MODE_BENT_NORMALS)
    const float3 N = firstRayDirection;
    const float3 origin = ray.origin + N * RAY_OFFSET;

    float3 accumDirection = float3(0.0, 0.0, 0.0);
    uint numUnoccluded = 0;

    for (uint sample_index = 0; sample_index < NUM_SAMPLES; sample_index++)
    {
        float2 rnd = float2(RandomFloat(ray_seed), RandomFloat(ray_seed));
        float3 direction = normalize(SampleCosineDir(rnd, N));

        payload.distance = -1.0;

        TraceScene(origin, direction, tmin, tmax, payload);

        if (payload.distance < 0.0)
        {
            accumDirection += direction;
            numUnoccluded++;
        }
    }

    float3 bentNormal = numUnoccluded > 0 ? normalize(accumDirection / float(numUnoccluded)) : N;

    float4 finalColor = float4(bentNormal, 1.0);
#else
    // shouldn't get here; output green so it's really obvious
    float4 finalColor = float4(0.0, 1.0, 0.0, 1.0);
#endif

    finalColor.a = saturate(finalColor.a);

    return finalColor;
}

#endif
