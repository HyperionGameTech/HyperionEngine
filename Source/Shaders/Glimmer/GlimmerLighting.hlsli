#ifndef GLIMMER_LIGHTING_HLSLI
#define GLIMMER_LIGHTING_HLSLI

#define GLIMMER_MAX_ALBEDO 0.9

#define GLIMMER_SKY_MIP 3.0

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 GlimmerSkyRadiance(float3 direction)
{
    const GlimmerSkyParams sky = GLIMMER_LIGHTING_SKY;

    if (sky.info.x == 0xFFFFu || sky.info.x == 0xFFFFFFFFu)
    {
        return (float3)0.0;
    }

    float3 radiance = envProbesColorTexture.SampleLevel(glimmerMaterialSampler, float4(direction, float(sky.info.x)), GLIMMER_SKY_MIP).rgb;
    radiance *= sky.params.x * world_shader_data.sky_light_params.x;

    const float luminance = GlimmerLuminance(radiance);

    return luminance > sky.params.y ? radiance * (sky.params.y / luminance) : radiance;
}

float3 GlimmerSkyIrradiance(float3 N)
{
    if (skyProbe.textureIndices == ~0u)
    {
        return (float3)0.0;
    }

    float shBands[9];
    ProjectSHBands(N, shBands);

    return max(EnvProbeSH(skyProbe, shBands), (float3)0.0) * skyProbe.world_position.w * world_shader_data.sky_light_params.x;
}

float GlimmerSkyReference()
{
    return max(GLIMMER_LIGHTING_SKY.params.z * world_shader_data.sky_light_params.x, 1e-4);
}

float3 GlimmerSunIrradiance()
{
    return world_shader_data.sun_color.rgb * world_shader_data.sun_direction_intensity.w;
}

float GlimmerCloudShadow(float3 P, float3 L)
{
    return GetCloudShadow(cloudWeatherMapTexture, cloudShadowMapTexture, glimmerMaterialSampler, cloudVolume, cloudWeatherMap, cloudShadowMap, P, L);
}

bool GlimmerFacesSun(float3 N)
{
    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);

    return dot(N, L) > 0.0 && L.y > -0.1;
}

float3 GlimmerIndirect(float3 P, float3 N)
{
    const float4 nearField = SampleGlimmerProbes(GLIMMER_LIGHTING_PROBES, P + N * 0.1, N);

    float4 farField = (float4)0.0;

    [branch]
    if (nearField.a < 0.999)
    {
        farField = EvaluateGlimmerSH(GLIMMER_LIGHTING_SH, P + N * 0.05, N);
    }

    const float4 previous = GlimmerBlendFarField(nearField, farField);

    return previous.rgb * previous.a + GlimmerSkyRadiance(N) * 0.5 * (1.0 - previous.a);
}

float3 GlimmerShade(float3 P, float3 N, float3 albedo, float sunVisibility, float3 indirect)
{
    albedo = min(albedo, (float3)GLIMMER_MAX_ALBEDO);

    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);

    float3 direct = (float3)0.0;

    if (GlimmerFacesSun(N) && sunVisibility > 0.0)
    {
        direct = GlimmerSunIrradiance() * dot(N, L) * HYP_FMATH_ONE_OVER_PI * sunVisibility * GlimmerCloudShadow(P, L);
    }

    return albedo * (direct + indirect);
}

float3 GlimmerShadeSurface(float3 P, float3 N, float3 albedo, float sunVisibility)
{
    return GlimmerShade(P, N, albedo, sunVisibility, GlimmerIndirect(P, N));
}

float3 GlimmerShadeRelit(float3 P, float3 albedo, float3 N, float4 relight)
{
    return GlimmerShade(P, N, albedo, relight.a, relight.rgb * GlimmerSkyReference());
}

#ifdef GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerShadeRelitHit(float3 P, float3 albedo, float3 N, bool isGroundHit, uint level, out float3 outRadiance)
{
    outRadiance = (float3)0.0;

    float4 relight;

    if (!GlimmerRelightCovers(isGroundHit, N)
        || !GlimmerSampleRelight(GLIMMER_LIGHTING_RELIGHT, GLIMMER_LIGHTING_GROUND, level, P.xz, isGroundHit ? GLIMMER_RELIGHT_GROUND : GLIMMER_RELIGHT_SPAN_TOP, relight))
    {
        return false;
    }

    outRadiance = GlimmerShadeRelit(P, albedo, N, relight);

    return true;
}

#endif

static float3 g_canopySurroundings = (float3)0.0;
static bool g_hasCanopySurroundings = false;

float3 GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    const float3 L = normalize(world_shader_data.sun_direction_intensity.xyz);

    float3 radiance = (float3)0.0;

    if (L.y > 0.0)
    {
        const float sunTransmittance = exp(-extinction * depthBelowTop / max(L.y, 0.15)) * GlimmerCloudShadow(P, L);

        radiance += GlimmerSunIrradiance() * sunTransmittance * (0.5 * HYP_FMATH_ONE_OVER_PI);
    }

    if (!g_hasCanopySurroundings)
    {
        g_canopySurroundings = GlimmerIndirect(P, float3(0.0, 1.0, 0.0));
        g_hasCanopySurroundings = true;
    }

    radiance += g_canopySurroundings;

    return min(albedo, (float3)GLIMMER_MAX_ALBEDO) * radiance;
}

#endif
