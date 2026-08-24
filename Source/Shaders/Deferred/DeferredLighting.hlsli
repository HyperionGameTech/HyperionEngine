#ifndef DEFERRED_LIGHTING_HLSLI
#define DEFERRED_LIGHTING_HLSLI

#include "../include/Shared.hlsli"
#include "../include/BRDF.hlsli"
#include "../include/Octahedron.hlsli"

struct Refraction
{
    float3 position;
    float3 direction;
};

void RefractionSolidSphere(
    float3 P, float3 N, float3 V, float eta_ir,
    out Refraction out_refraction)
{
    const float thickness = 0.8;

    const float3 R = refract(-V, N, eta_ir);
    float NdotR = dot(N, R);
    float d = thickness * -NdotR;
    float3 n1 = normalize(NdotR * R - N * 0.5);

    Refraction refraction;
    refraction.position = float3(P + R * d);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}

#ifndef HYP_DEFERRED_NO_REFRACTION

float3 CalculateRefraction(
    uint2 image_dimensions,
    float3 P, float3 N, float3 V, float2 texcoord,
    float3 F0, float3 E,
    float transmission, float roughness,
    float4 opaque_color, float4 translucent_color,
    float3 brdf)
{
    // dimensions of mip chain image
    const uint max_dimension = max(image_dimensions.x, image_dimensions.y);

    const float IOR = 1.5;
    const float air_ior = 1.0;
    const float eta_ir = air_ior / IOR;

    Refraction refraction;
    RefractionSolidSphere(P, N, V, eta_ir, refraction);

    float4 refraction_pos = mul(camera.viewProjMat, float4(refraction.position, 1.0));
    refraction_pos /= refraction_pos.w;

    float2 refraction_texcoord = float2(refraction_pos.x * 0.5 + 0.5, (-refraction_pos.y) * 0.5 + 0.5);

    const float lod = ApplyIORToRoughness(IOR, roughness) * log2(float(max_dimension));

    float absorption = 0.1; // TODO: material parameter
    float3 T = min(float3(1.0, 1.0, 1.0), exp(-absorption * refraction.direction));

    float3 Ft = SAMPLE_TEXTURE_2D_LOD(sampler_linear, GBufferMipChain, refraction_texcoord, lod).rgb;
    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E; // energy conservation: subtract the fraction already lost to reflection
    Ft *= T;

    return Ft;
}

#endif // HYP_DEFERRED_NO_REFRACTION

#ifndef HYP_DEFERRED_NO_ENV_PROBE

#include "../include/EnvProbes.hlsli"

void ApplyReflectionProbe(uint probe_texture_index, float3 R, float lod, inout float4 ibl)
{
    ibl = float4(0.0, 0.0, 0.0, 0.0);
    probe_texture_index = min(probe_texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1);

    ibl = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(sampler_linear, envProbesColorTexture, float4(R, float(probe_texture_index)), lod);
}

void ApplyReflectionProbe(uint probe_texture_index, float3 probe_world_position, float3 aabb_min, float3 aabb_max, float3 P, float3 R, float lod, inout float4 ibl)
{
    ibl = float4(0.0, 0.0, 0.0, 0.0);

    probe_texture_index = min(probe_texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1);

#ifdef ENV_PROBE_PARALLAX_CORRECTED
    const float3 extent = (aabb_max - aabb_min);
    const float3 center = (aabb_max + aabb_min) * 0.5;
    const float3 diff = P - center;

    R = EnvProbeCoordParallaxCorrected(probe_world_position, aabb_min, aabb_max, P, R);
    R = normalize(R);
#endif // ENV_PROBE_PARALLAX_CORRECTED

    ibl = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(sampler_linear, envProbesColorTexture, float4(R, float(probe_texture_index)), lod);
}

float4 CalculateReflectionProbe(in EnvProbe probe, float3 P, float3 N, float3 R, float3 camera_position, float roughness)
{
    float4 ibl = float4(0.0, 0.0, 0.0, 0.0);

    const float lod = HYP_FMATH_SQR(roughness) * 7.0;

#ifndef ENV_PROBE_PARALLAX_CORRECTED
    // ENV_PROBE_PARALLAX_CORRECTED is not statically defined, we need to use flags on the EnvProbe struct
    // at render time to determine if the probe is parallax corrected.
    const bool is_parallax_corrected = bool(GET_ENV_PROBE_FLAGS(probe) & EPF_PARALLAX_CORRECTED);

    if (is_parallax_corrected)
    {
        R = EnvProbeCoordParallaxCorrected(probe.world_position.xyz, probe.aabb_min.xyz, probe.aabb_max.xyz, P, R);
    }
#endif // ENV_PROBE_PARALLAX_CORRECTED

    const uint colorTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(probe);
    if (colorTextureIndex != INVALID_ENV_PROBE_TEXTURE)
    {
        ApplyReflectionProbe(colorTextureIndex, probe.world_position.xyz, probe.aabb_min.xyz, probe.aabb_max.xyz, P, R, lod, ibl);
    }

    return ibl;
}

// Must include ClusteredShading.hlsli before including this if you want
// to use EvaluateEnvProbes.

// Define for intellisense on the block below
#ifndef HYP_SHADER_COMPILER
#define CLUSTERED_SHADING_HLSLI
#endif // HYP_SHADER_COMPILER

#ifdef CLUSTERED_SHADING_HLSLI

float CalculateProbeVisibility(
    float3 probeToPoint, float dist, float3 N,
    float far,
    uint visTextureIndex)
{
    const float3 probeToPointN = probeToPoint / dist;

    const float distNorm = dist / max(far, HYP_FMATH_EPSILON);

    const float2 moments = envProbesDepthTexture.SampleLevel(
        sampler_linear, float4(probeToPointN, float(visTextureIndex)), 0).rg;

    static const float s_selfShadowBias = 0.02;
    static const float s_minVariance = 1e-2;
    static const float s_softenAmount = 0.05;

    float variance = max(moments.y - moments.x * moments.x, s_minVariance);
    
    float d = max(distNorm - moments.x - s_selfShadowBias, 0.0);
    float p_max = variance / (variance + d * d);

    float visibility = saturate((p_max - s_softenAmount) / (1.0 - s_softenAmount));

    float directionalWeight = max(HYP_FMATH_EPSILON, (dot(-probeToPointN, N) + 1.0) * 0.5);
    visibility *= directionalWeight;

    return saturate(visibility);
}

void EvaluateSingleProbe(
    float3 positionVS, float3 positionWS,
    float3 N, float3 V, float3 R,
    float perceptualRoughness,
    float lightmappedWeight,
    in EnvProbe inProbe,
    inout float3 reflectionsSum, inout float reflectionsWeightSum,
    inout float3 irradianceSum, inout float irradianceWeightSum)
{
#define CURRENT_ENV_PROBE inProbe

    const uint envProbeFlags = GET_ENV_PROBE_FLAGS(CURRENT_ENV_PROBE);

    const uint probeType = GET_ENV_PROBE_TYPE(CURRENT_ENV_PROBE);

    const bool isIrradianceProbe = (probeType == EPT_AMBIENT);

    const uint textureIndices = CURRENT_ENV_PROBE.textureIndices;
    const uint colorTextureIndex = (textureIndices & 0xFFFFu);
    const uint visTextureIndex = (textureIndices >> 16) & 0xFFFFu;

    const float numMips = 7.0; // assuming 128x128 cubemap size for reflection probes
    const float lod = perceptualRoughness * numMips;

    const float4 aabbMin = CURRENT_ENV_PROBE.aabb_min;
    const float4 aabbMax = CURRENT_ENV_PROBE.aabb_max;

    const float4 worldPosition = CURRENT_ENV_PROBE.world_position;
    const float3 worldPosition3 = worldPosition.xyz;
    const float diffuseStrength = worldPosition.w;

    const float3 probeReflectionVector = bool(envProbeFlags & EPF_PARALLAX_CORRECTED)
            ? normalize(EnvProbeCoordParallaxCorrected(worldPosition3, aabbMin.xyz, aabbMax.xyz, positionWS, R))
            : R;

    float4 currentReflections = (float4) 0;
    float4 currentIrradiance = EnvProbeSH(CURRENT_ENV_PROBE, N);

    float3 probeToPoint = positionWS - worldPosition3;
    float dist = length(probeToPoint);

    float far = aabbMax.w;
    float visibility = 1.0;

    if ((envProbeFlags & EPF_VISIBILITY) && visTextureIndex != INVALID_ENV_PROBE_TEXTURE)
    {
        visibility = CalculateProbeVisibility(probeToPoint, dist, N, far, visTextureIndex);
    }

    ApplyReflectionProbe(
            colorTextureIndex,
            probeReflectionVector,
            lod,
            currentReflections);

    currentReflections.a = saturate(currentReflections.a);

    // dont show where we have lightmaps!
    const float irradianceOnlyWeight = (float) isIrradianceProbe;
    const float diffuseContributionWeight = (1.0 - lightmappedWeight) * diffuseStrength;

    // @TODO Make configurable.
    static const float kIrradianceProbeBlendFactor = 0.04;
    static const float kReflectionsProbeBlendFactor = 0.02;
    const float blendFactor = lerp(kReflectionsProbeBlendFactor, kIrradianceProbeBlendFactor, irradianceOnlyWeight);

    const float boundsWeight = CalculateEnvProbeWeight(positionWS, aabbMin.xyz, aabbMax.xyz, blendFactor);
    
    const float reflectionsWeight = max(0.0, boundsWeight * visibility * (1.0 - irradianceOnlyWeight) * currentReflections.a);
    const float irradianceWeight = max(0.0, boundsWeight * visibility * diffuseContributionWeight * currentIrradiance.a);

    reflectionsSum += currentReflections.rgb * reflectionsWeight;
    reflectionsWeightSum += reflectionsWeight;

    irradianceSum += currentIrradiance.rgb * irradianceWeight;
    irradianceWeightSum += irradianceWeight;

#undef CURRENT_ENV_PROBE
}

void EvaluateEnvProbes(
    float3 positionVS, float3 positionWS,
    float3 N, float3 V, float3 R,
    float nearClip, float farClip,
    float roughness, float perceptualRoughness,
    float2 texcoordSS, uint2 dimensions,
    uint inMask,
    inout float4 reflections, inout float4 irradiance)
{
    const uint2 pixelCoord = uint2(texcoordSS * dimensions);

    const uint gridIndex = Cluster_GetGridIndex(
        dimensions, pixelCoord,
        positionVS.z,
        nearClip, farClip);

    const uint2 clusterData = ClusterGridBuffer.Load2(gridIndex * sizeof(uint2));

    const uint clusterIndexOffset = clusterData.x;

    const uint numLights = (clusterData.y & 0xFFFF);
    const uint numEnvProbes = (clusterData.y >> 16) & 0xFFFF;

    //////////////////////////////////////////////////

    // For masking out lightmapped elements so probes don't affect them
    // 0.0 == Not Lightmapped, 1.0 == lightmapped.
    const float lightmappedWeight = min(1.0, float(inMask & OBJECT_MASK_LIGHTMAPPED));

    float3 reflectionsSum = (float3) 0.0;
    float reflectionsWeightSum = 0.0;

    float3 irradianceSum = (float3) 0.0;
    float irradianceWeightSum = 0.0;

    for (uint currentProbeIndex = numEnvProbes; currentProbeIndex != 0; --currentProbeIndex)
    {
        const uint envProbeIndex = Cluster_LoadEnvProbeIndex(clusterIndexOffset, numLights, currentProbeIndex - 1);

        EvaluateSingleProbe(
            positionVS, positionWS,
            N, V, R,
            perceptualRoughness,
            lightmappedWeight,
            EnvProbesBuffer[envProbeIndex],
            reflectionsSum, reflectionsWeightSum,
            irradianceSum, irradianceWeightSum);
    }

    // to get that good intellisense
#ifndef HYP_SHADER_COMPILER
#define DEFERRED_LIGHTING_HAS_SKY
#define skyProbe ((EnvProbe)0)
#endif

#ifdef DEFERRED_LIGHTING_HAS_SKY

    // ~0u == ((INVALID_ENV_PROBE_TEXTURE << 16) | INVALID_ENV_PROBE_TEXTURE)
    // (all invalid)
    if (skyProbe.textureIndices != ~0u)
    {
        float3 skyReflectionsSum = (float3) 0.0;
        float skyReflectionsWeightSum = 0.0;

        float3 skyIrradianceSum = (float3) 0.0;
        float skyIrradianceWeightSum = 0.0;

        EvaluateSingleProbe(
            positionVS, positionWS,
            N, V, R,
            perceptualRoughness,
            lightmappedWeight,
            skyProbe,
            skyReflectionsSum, skyReflectionsWeightSum,
            skyIrradianceSum, skyIrradianceWeightSum);

        const float reflectionsResidual = max(0.0, 1.0 - reflectionsWeightSum);
        const float irradianceResidual = max(0.0, 1.0 - irradianceWeightSum);
        
        const float skyReflectionsEffectiveWeight = min(skyReflectionsWeightSum, reflectionsResidual);
        reflectionsSum += (skyReflectionsSum / max(skyReflectionsWeightSum, HYP_FMATH_EPSILON)) * skyReflectionsEffectiveWeight;
        reflectionsWeightSum += skyReflectionsEffectiveWeight;

        const float skyIrradianceEffectiveWeight = min(skyIrradianceWeightSum, irradianceResidual);
        irradianceSum += (skyIrradianceSum / max(skyIrradianceWeightSum, HYP_FMATH_EPSILON)) * skyIrradianceEffectiveWeight;
        irradianceWeightSum += skyIrradianceEffectiveWeight;

    }
#endif // DEFERRED_LIGHTING_HAS_SKY

    //////////////////////////////////////////////////
    
    reflections = float4(reflectionsSum / max(reflectionsWeightSum, HYP_FMATH_EPSILON), saturate(reflectionsWeightSum));
    irradiance = float4(irradianceSum / max(irradianceWeightSum, HYP_FMATH_EPSILON), saturate(irradianceWeightSum));

    // DEBUG
    reflections = any(isnan(reflections)) ? (float4) 0 : reflections;
}

#endif // CLUSTERED_SHADING_HLSLI

#endif // HYP_DEFERRED_NO_ENV_PROBE

#ifndef HYP_DEFERRED_NO_RT_RADIANCE
#ifdef PATHTRACER
float4 CalculatePathTracing(float2 uv)
{
    return SAMPLE_TEXTURE_2D_LOD(sampler_linear, RTRadianceResultTexture, uv, 0.0);
}
#endif // PATHTRACER

#ifdef RT_REFLECTIONS
void CalculateRayTracingReflection(float2 uv, inout float4 reflections)
{
    float4 rt_radiance = SAMPLE_TEXTURE_2D_LOD(sampler_linear, RTRadianceResultTexture, uv, 0.0);

    reflections = reflections * (1.0 - rt_radiance.a) + float4(rt_radiance.rgb, 1.0) * rt_radiance.a;
}
#endif // RT_REFLECTIONS
#endif // HYP_DEFERRED_NO_RT_RADIANCE

void IntegrateReflections(inout float3 Fr, in float4 reflections)
{
    Fr = (Fr * (1.0 - reflections.a)) + (reflections.rgb);
}

#endif // DEFERRED_LIGHTING_HLSLI
