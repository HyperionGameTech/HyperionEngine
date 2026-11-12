#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/Packing.hlsli"

// TRACE adds a frame of paths per pixel and shows the result, STATS sums the error per group, REDUCE sums the groups
PERMUTE(MODE, TRACE, STATS, REDUCE)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#include "../Include/Scene.hlsli"
#include "../Include/Gbuffer.hlsli"
#include "../Include/EnvProbes.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerCommon.hlsli"

#define GLIMMER_REFERENCE_VIEW_REFERENCE 1u
#define GLIMMER_REFERENCE_VIEW_GLIMMER 2u
#define GLIMMER_REFERENCE_VIEW_ERROR 3u
#define GLIMMER_REFERENCE_VIEW_SPLIT 4u

#define GLIMMER_REFERENCE_CLASSES 5u
#define GLIMMER_REFERENCE_CLASS_ALL 0u
#define GLIMMER_REFERENCE_CLASS_STATIC 1u
#define GLIMMER_REFERENCE_CLASS_FOLIAGE 2u
#define GLIMMER_REFERENCE_CLASS_DYNAMIC 3u
#define GLIMMER_REFERENCE_CLASS_TERRAIN 4u

#define GLIMMER_REFERENCE_CLASS_VALUES 6u
#define GLIMMER_REFERENCE_STATS 30u
#define GLIMMER_REFERENCE_STATS_STRIDE 32u

#define GLIMMER_REFERENCE_STATS_GROUP 16u

struct GlimmerReferenceConstants
{
    uint4 dimensions;     // xy = output size, z = frame seed, w = frames accumulated before this one
    uint4 info;           // x = sky probe color texture index, y = traced instances, z = GLIMMER_REFERENCE_VIEW_*, w = paths per pixel this frame
    uint4 stats;          // x = groups STATS wrote, y = groups along x, z = pixels apart the reference traces along each axis
    float4 toSun;         // xyz = toward the sun
    float4 sunIrradiance; // rgb
    float4 params;        // x = foliage extinction, y = sky radiance scale, z = bounces, w = albedo where the ground's isn't known
    float4 display;       // x = exposure, y = error the heat view saturates at, z = 1 for the furnace test (white sky and surfaces, no sun or emitters), w = 1 to show them shaded
    float4 skySH[9];      // rgb = the sky's radiance SH Glimmer composes with; skySH[0].w = 1 to light escaping paths from it rather than the cubemap
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
};

DECLARE_BUFFER_DYNAMIC(GlimmerReference, CBuffer) cbuffer CBuffer
{
    GlimmerReferenceConstants constants;
};

#if defined(MODE_REDUCE)
DECLARE_SRV(GlimmerReference, GlimmerReferencePartialsBuffer) StructuredBuffer<float> glimmerReferencePartials;
DECLARE_UAV(GlimmerReference, OutStats) RWStructuredBuffer<float> OutStats;

[numthreads(GLIMMER_REFERENCE_STATS_STRIDE, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint value = dispatchThreadId.x;

    float sum = 0.0;

    [loop]
    for (uint group = 0; group < constants.stats.x; group++)
    {
        sum += glimmerReferencePartials[group * GLIMMER_REFERENCE_STATS_STRIDE + value];
    }

    OutStats[value] = sum;
}
#else

DECLARE_SRV(GlimmerReference, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(GlimmerReference, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(GlimmerReference, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;

DECLARE_SRV_DYNAMIC(GlimmerReference, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(GlimmerReference, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;

#include "GlimmerGround.hlsli"

float GlimmerReferenceLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

struct GlimmerReferencePixel
{
    float3 P;
    float3 N;
    float transmission;
    uint classIndex;
    bool isValid;
};

GlimmerReferencePixel GlimmerReferenceLoadPixel(int2 pixel)
{
    GlimmerReferencePixel result;

    const uint2 dimensions = constants.dimensions.xy;
    const float depth = GBufferDepthTexture.Load(int3(pixel, 0)).r;

    const float2 texcoord = (float2(pixel) + 0.5) / float2(dimensions);
    const float2 unjitteredTexcoord = texcoord - float2(camera.jitter.x, -camera.jitter.y) * 0.5;

    float4 positionWS = mul(camera.invViewMat, ReconstructViewSpacePositionFromDepth(camera.invProjMat, unjitteredTexcoord, depth));
    positionWS /= positionWS.w;

    result.P = positionWS.xyz;
    result.N = normalize(GBufferUnpackNormal(GBufferNormalsTexture.Load(int3(pixel, 0))));

    const uint materialBits = GBufferMaterialTexture.Load(int3(pixel, 0));
    const uint objectMask = GBufferUnpackObjectMask(materialBits);

    result.isValid = depth < 1.0 && ((materialBits >> 28u) & OBJECT_MASK_LIGHTMAPPED) == 0u;
    result.transmission = 0.0;

    if ((objectMask & OBJECT_MASK_FOLIAGE) != 0u)
    {
        result.classIndex = GLIMMER_REFERENCE_CLASS_FOLIAGE;
        result.transmission = GBufferUnpackTransmission(materialBits);
    }
    else if ((objectMask & OBJECT_MASK_DYNAMIC) != 0u)
    {
        result.classIndex = GLIMMER_REFERENCE_CLASS_DYNAMIC;
    }
    else
    {
        float groundHeight;
        uint groundLevel;

        const bool isOnGround = GlimmerSampleGround(constants.ground, result.P.xz, 0u, groundHeight, groundLevel) && result.P.y <= groundHeight + 0.2;

        result.classIndex = isOnGround ? GLIMMER_REFERENCE_CLASS_TERRAIN : GLIMMER_REFERENCE_CLASS_STATIC;
    }

    return result;
}

#if defined(MODE_TRACE)
DECLARE_SRV(GlimmerReference, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerReference, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerReference, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerReference, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerReference, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;

#include "../Include/RayTracing/BVH.hlsli"
#include "BVH/GlimmerBVHCommon.hlsli"

DECLARE_SRV(GlimmerReference, GlimmerTLASNodesBuffer) StructuredBuffer<BVHNode> glimmerTLASNodes;
DECLARE_SRV(GlimmerReference, GlimmerTracedInstancesBuffer) StructuredBuffer<GlimmerTracedInstance> glimmerTracedInstances;
DECLARE_SRV(GlimmerReference, GlimmerBLASNodesBuffer) StructuredBuffer<BVHBLASNode> glimmerBLASNodes;
DECLARE_SRV(GlimmerReference, GlimmerTrianglesBuffer) StructuredBuffer<uint> glimmerTriangles;

#include "BVH/GlimmerBVHTrace.hlsli"

DECLARE_SRV(GlimmerReference, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(GlimmerReference, GlimmerIrradianceTexture) Texture2D GlimmerIrradianceTexture;
DECLARE_SRV(GlimmerReference, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;

DECLARE_UAV(GlimmerReference, OutReference) RWTexture2D<float4> OutReference;
DECLARE_UAV(GlimmerReference, OutGlimmer) RWTexture2D<float4> OutGlimmer;
DECLARE_UAV(GlimmerReference, OutShadedReference) RWTexture2D<float4> OutShadedReference;
DECLARE_UAV(GlimmerReference, OutShadedGlimmer) RWTexture2D<float4> OutShadedGlimmer;
DECLARE_UAV(GlimmerReference, OutImage) RWTexture2D<float4> OutImage;

#include "GlimmerMaterial.hlsli"
#include "GlimmerLight.hlsli"

GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo, GlimmerLight skyTransfer)
{
    return GlimmerNoLight();
}

#define GLIMMER_HEIGHTFIELD_FREE_FLIGHT
#include "GlimmerHeightfield.hlsli"

#define GLIMMER_REFERENCE_MAX_DISTANCE 2000.0
#define GLIMMER_REFERENCE_BIAS 0.02
#define GLIMMER_REFERENCE_BIAS_PER_METER 0.002
#define GLIMMER_REFERENCE_ROULETTE_BOUNCE 3u

#define GLIMMER_REFERENCE_MISS 0u
#define GLIMMER_REFERENCE_SURFACE 1u
#define GLIMMER_REFERENCE_CANOPY 2u
#define GLIMMER_REFERENCE_ABSORBED 3u

uint GlimmerReferenceNext(inout uint state)
{
    state = state * 747796405u + 2891336453u;

    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

float GlimmerReferenceRandom(inout uint state)
{
    return float(GlimmerReferenceNext(state) >> 8u) * (1.0 / 16777216.0);
}

void GlimmerReferenceBasis(float3 N, out float3 T, out float3 B)
{
    const float sign = N.z >= 0.0 ? 1.0 : -1.0;
    const float a = -1.0 / (sign + N.z);
    const float b = N.x * N.y * a;

    T = float3(1.0 + sign * N.x * N.x * a, sign * b, -sign * N.x);
    B = float3(b, sign + N.y * N.y * a, -N.y);
}

float3 GlimmerReferenceCosineDirection(float3 N, inout uint rng)
{
    const float u = GlimmerReferenceRandom(rng);
    const float phi = 2.0 * HYP_FMATH_PI * GlimmerReferenceRandom(rng);
    const float r = sqrt(u);

    float3 T;
    float3 B;
    GlimmerReferenceBasis(N, T, B);

    return normalize(T * (r * cos(phi)) + B * (r * sin(phi)) + N * sqrt(max(1.0 - u, 0.0)));
}

float3 GlimmerReferenceSphereDirection(inout uint rng)
{
    const float z = 1.0 - 2.0 * GlimmerReferenceRandom(rng);
    const float phi = 2.0 * HYP_FMATH_PI * GlimmerReferenceRandom(rng);
    const float r = sqrt(max(1.0 - z * z, 0.0));

    return float3(r * cos(phi), r * sin(phi), z);
}

// how far the BVH holds the solids along the ray
float GlimmerReferenceSolidsReach(float3 origin, float3 direction)
{
    return min(GlimmerBVHWindowExit(origin, direction, constants.info.y), GLIMMER_REFERENCE_MAX_DISTANCE);
}

float GlimmerReferenceSunVisibility(float3 origin, float bias)
{
    const float3 toSun = constants.toSun.xyz;
    const float reach = GlimmerReferenceSolidsReach(origin, toSun);

    if (reach > 0.0)
    {
        GlimmerBVHHit bvhHit;
        GlimmerBVHStats stats = GlimmerMakeBVHStats();

        if (TraceGlimmerBVH(origin, toSun, bias, reach, constants.info.y, true, bvhHit, stats))
        {
            return 0.0;
        }
    }

    glimmerScatterTransmittance = 0.0;

    GlimmerHeightfieldHit heightfieldHit;

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, toSun, GLIMMER_REFERENCE_MAX_DISTANCE, 0u, reach, constants.params.x, false, heightfieldHit))
    {
        return 0.0;
    }

    return heightfieldHit.transmittance * heightfieldHit.scatterTransmittance;
}

struct GlimmerReferenceHit
{
    uint kind;
    float3 P;
    float3 N;
    float3 albedo;
    float3 emitted;
    float transmittance;
    bool isMedium; // a canopy or partly solid span the ray scattered in, rather than a surface
};

GlimmerReferenceHit GlimmerReferenceTrace(float3 origin, float3 direction, float bias, inout uint rng)
{
    GlimmerReferenceHit result;
    result.kind = GLIMMER_REFERENCE_MISS;
    result.P = origin;
    result.N = -direction;
    result.albedo = (float3)0.0;
    result.emitted = (float3)0.0;
    result.transmittance = 1.0;
    result.isMedium = false;

    const float reach = GlimmerReferenceSolidsReach(origin, direction);

    float hitT = GLIMMER_REFERENCE_MAX_DISTANCE;

    GlimmerBVHHit bvhHit;
    bool hitBVH = false;

    if (reach > 0.0)
    {
        GlimmerBVHStats stats = GlimmerMakeBVHStats();

        hitBVH = TraceGlimmerBVH(origin, direction, bias, reach, constants.info.y, false, bvhHit, stats);

        if (hitBVH)
        {
            hitT = bvhHit.t;
        }
    }

    glimmerScatterTransmittance = GlimmerReferenceRandom(rng);

    GlimmerHeightfieldHit heightfieldHit;

    const bool hitHeightfield = GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, hitT, 0u, reach, constants.params.x, false, heightfieldHit);

    result.transmittance = heightfieldHit.transmittance;

    if (hitHeightfield)
    {
        result.P = origin + direction * heightfieldHit.t;

        if (heightfieldHit.t <= 0.0)
        {
            result.kind = GLIMMER_REFERENCE_ABSORBED;
        }
        else if (heightfieldHit.kind == GLIMMER_HEIGHTFIELD_CANOPY)
        {
            result.kind = GLIMMER_REFERENCE_CANOPY;
            result.albedo = heightfieldHit.albedo;
        }
        else
        {
            result.kind = GLIMMER_REFERENCE_SURFACE;
            result.N = heightfieldHit.normal;
            result.isMedium = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_SOLID;

            result.albedo = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, result.P.xz, heightfieldHit.level, (float3)constants.params.w)
                : heightfieldHit.albedo;
        }

        return result;
    }

    if (hitBVH)
    {
        const GlimmerTracedInstance instance = glimmerTracedInstances[bvhHit.instanceIndex];

        result.P = origin + direction * bvhHit.t;

        if (!bvhHit.frontFace && (instance.data.w & (GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED | GLIMMER_INSTANCE_FLAG_ALPHA_TESTED)) == 0u)
        {
            result.kind = GLIMMER_REFERENCE_ABSORBED;

            return result;
        }

        result.kind = GLIMMER_REFERENCE_SURFACE;
        result.N = GlimmerGetHitNormal(bvhHit, direction);
        result.albedo = GlimmerGetMaterialAverageAlbedo(instance.data.z);
        result.emitted = GlimmerGetMaterialEmissive(instance.data.z);
    }

    return result;
}

float3 GlimmerReferenceSky(float3 direction)
{
    if (constants.skySH[0].w != 0.0)
    {
        float basis[GLIMMER_LIGHT_TERMS];
        GlimmerLightBasis(direction, basis);

        float3 radiance = (float3)0.0;

        [unroll]
        for (uint term = 0; term < 9u; term++)
        {
            radiance += constants.skySH[term].rgb * basis[term];
        }

        return max(radiance, (float3)0.0);
    }

    if (constants.info.x >= INVALID_ENV_PROBE_TEXTURE)
    {
        return (float3)0.0;
    }

    return EnvProbeSample(glimmerMaterialSampler, envProbesColorTexture, constants.info.x, direction, 0.0).rgb * constants.params.y;
}

// irradiance / pi reaching P from the hemisphere around N: the mean radiance of cosine-distributed paths
float3 GlimmerReferencePath(float3 P, float3 N, float bias, inout uint rng)
{
    float3 origin = P + N * bias;
    float3 direction = GlimmerReferenceCosineDirection(N, rng);

    float3 throughput = (float3)1.0;
    float3 radiance = (float3)0.0;

    const uint bounces = uint(constants.params.z);

    const bool isFurnace = constants.display.z != 0.0;
    const float3 sunIrradiance = isFurnace ? (float3)0.0 : constants.sunIrradiance.rgb;

    [loop]
    for (uint bounce = 0; bounce <= bounces; bounce++)
    {
        const GlimmerReferenceHit hit = GlimmerReferenceTrace(origin, direction, bias, rng);

        throughput *= hit.transmittance;

        if (hit.kind == GLIMMER_REFERENCE_MISS)
        {
            radiance += throughput * (isFurnace ? (float3)1.0 : GlimmerReferenceSky(direction));

            break;
        }

        if (hit.kind == GLIMMER_REFERENCE_ABSORBED || bounce == bounces)
        {
            break;
        }

        const float3 albedo = isFurnace && hit.kind == GLIMMER_REFERENCE_SURFACE && !hit.isMedium ? (float3)1.0 : min(hit.albedo, (float3)0.98);

        if (!isFurnace)
        {
            radiance += throughput * hit.emitted;
        }

        if (hit.kind == GLIMMER_REFERENCE_CANOPY)
        {
            origin = hit.P;

            // isotropic: the sun's irradiance spread over the sphere
            radiance += throughput * albedo * sunIrradiance * (GlimmerReferenceSunVisibility(origin, 0.0) / (4.0 * HYP_FMATH_PI));

            direction = GlimmerReferenceSphereDirection(rng);
        }
        else
        {
            const float hitBias = GLIMMER_REFERENCE_BIAS + GLIMMER_REFERENCE_BIAS_PER_METER * length(hit.P - camera.position.xyz);

            origin = hit.P + hit.N * hitBias;

            const float sunCosine = dot(hit.N, constants.toSun.xyz);

            if (sunCosine > 0.0 && !isFurnace)
            {
                radiance += throughput * albedo * sunIrradiance * (sunCosine * HYP_FMATH_ONE_OVER_PI * GlimmerReferenceSunVisibility(origin, hitBias));
            }

            direction = GlimmerReferenceCosineDirection(hit.N, rng);
        }

        throughput *= albedo;

        if (bounce + 1u >= GLIMMER_REFERENCE_ROULETTE_BOUNCE)
        {
            const float survival = min(max(throughput.r, max(throughput.g, throughput.b)), 0.95);

            if (GlimmerReferenceRandom(rng) >= survival)
            {
                break;
            }

            throughput /= survival;
        }
    }

    return radiance;
}

float3 GlimmerReferenceTonemap(float3 color)
{
    const float3 exposed = color * constants.display.x;

    return exposed / (1.0 + exposed);
}

float3 GlimmerReferenceHeat(float relativeError)
{
    const float t = saturate(abs(relativeError) / max(constants.display.y, 1e-4));

    return relativeError < 0.0 ? lerp((float3)0.1, float3(0.1, 0.35, 1.0), t) : lerp((float3)0.1, float3(1.0, 0.25, 0.1), t);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const int2 pixel = int2(dispatchThreadId.xy);

    if (any(dispatchThreadId.xy >= constants.dimensions.xy))
    {
        return;
    }

    // only every stride-th pixel along each axis is traced; the rest show the traced pixel they fall to
    const int stride = int(max(constants.stats.z, 1u));
    const int2 tracedPixel = (pixel / stride) * stride;
    const bool isTraced = all(pixel == tracedPixel);

    const GlimmerReferencePixel surface = GlimmerReferenceLoadPixel(tracedPixel);

    const uint framesBefore = constants.dimensions.w;

    float4 referenceSum = framesBefore != 0u ? OutReference[tracedPixel] : (float4)0.0;
    float4 glimmerSum = framesBefore != 0u ? OutGlimmer[tracedPixel] : (float4)0.0;
    float4 shadedReferenceSum = framesBefore != 0u ? OutShadedReference[tracedPixel] : (float4)0.0;
    float4 shadedGlimmerSum = framesBefore != 0u ? OutShadedGlimmer[tracedPixel] : (float4)0.0;

    if (isTraced && surface.isValid)
    {
        uint rng = uint(pixel.x) + uint(pixel.y) * constants.dimensions.x;
        rng = GlimmerReferenceNext(rng) ^ constants.dimensions.z;
        rng = GlimmerReferenceNext(rng);

        const float bias = GLIMMER_REFERENCE_BIAS + GLIMMER_REFERENCE_BIAS_PER_METER * length(surface.P - camera.position.xyz);
        const uint paths = max(constants.info.w, 1u);

        float3 estimate = (float3)0.0;

        for (uint pathIndex = 0; pathIndex < paths; pathIndex++)
        {
            float3 N = surface.N;
            float weight = 1.0;

            // E(N) + transmission E(-N), picking a side in proportion so both sides weigh 1 + transmission
            if (surface.transmission > 0.0)
            {
                weight = 1.0 + surface.transmission;

                if (GlimmerReferenceRandom(rng) * weight >= 1.0)
                {
                    N = -N;
                }
            }

            estimate += GlimmerReferencePath(surface.P + surface.N * (float(constants.stats.w) * 0.01), N, bias, rng) * weight;
        }

        estimate /= float(paths);

        const float3 glimmer = GlimmerIrradianceTexture.Load(int3(pixel, 0)).rgb;

        referenceSum += float4(estimate, GlimmerReferenceLuminance(estimate) * GlimmerReferenceLuminance(estimate));
        glimmerSum += float4(glimmer, GlimmerReferenceLuminance(glimmer) * GlimmerReferenceLuminance(glimmer));

        // diffuse only: the sun behind a traced shadow ray plus the irradiance, both / pi, times what the surface reflects
        const float4 normalSample = GBufferNormalsTexture.Load(int3(pixel, 0));

        GBufferMaterialParams materialParams;
        GBufferUnpackMaterialParams(normalSample.x, 0u, materialParams);

        const float3 diffuseColor = GBufferAlbedoTexture.Load(int3(pixel, 0)).rgb * (1.0 - materialParams.metalness);

        float3 direct = (float3)0.0;
        const float sunCosine = dot(surface.N, constants.toSun.xyz);

        if (sunCosine > 0.0)
        {
            direct = constants.sunIrradiance.rgb * (sunCosine * HYP_FMATH_ONE_OVER_PI * GlimmerReferenceSunVisibility(surface.P + surface.N * bias, bias));
        }

        shadedReferenceSum.rgb += diffuseColor * (direct + estimate);
        shadedGlimmerSum.rgb += diffuseColor * (direct + glimmer);
    }
    else if (isTraced && GBufferDepthTexture.Load(int3(pixel, 0)).r >= 1.0)
    {
        const float2 texcoord = (float2(pixel) + 0.5) / float2(constants.dimensions.xy);

        float4 farWS = mul(camera.invViewMat, ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, 1.0));
        farWS /= farWS.w;

        const float3 sky = GlimmerReferenceSky(normalize(farWS.xyz - camera.position.xyz));

        shadedReferenceSum.rgb += sky;
        shadedGlimmerSum.rgb += sky;
    }

    if (isTraced)
    {
        OutReference[pixel] = referenceSum;
        OutGlimmer[pixel] = glimmerSum;
        OutShadedReference[pixel] = shadedReferenceSum;
        OutShadedGlimmer[pixel] = shadedGlimmerSum;
    }

    const float frames = float(framesBefore + 1u);

    const bool isShaded = constants.display.w != 0.0;

    const float3 reference = (isShaded ? shadedReferenceSum.rgb : referenceSum.rgb) / frames;
    const float3 glimmer = (isShaded ? shadedGlimmerSum.rgb : glimmerSum.rgb) / frames;

    float3 color = (float3)0.0;

    const uint view = constants.info.z;

    if (!surface.isValid && !isShaded)
    {
        color = (float3)0.0;
    }
    else if (view == GLIMMER_REFERENCE_VIEW_GLIMMER || (view == GLIMMER_REFERENCE_VIEW_SPLIT && pixel.x >= int(constants.dimensions.x / 2u)))
    {
        color = GlimmerReferenceTonemap(glimmer);
    }
    else if (view == GLIMMER_REFERENCE_VIEW_ERROR)
    {
        const float referenceLuminance = GlimmerReferenceLuminance(reference);

        color = GlimmerReferenceHeat((GlimmerReferenceLuminance(glimmer) - referenceLuminance) / max(referenceLuminance, 1e-3));
    }
    else
    {
        color = GlimmerReferenceTonemap(reference);
    }

    if (isShaded && view != GLIMMER_REFERENCE_VIEW_ERROR)
    {
        color = pow(max(color, (float3)0.0), (float3)(1.0 / 2.2));
    }

    OutImage[pixel] = float4(color, 1.0);
}

#elif defined(MODE_STATS)
DECLARE_SRV(GlimmerReference, GlimmerReferenceTexture) Texture2D<float4> glimmerReference;
DECLARE_SRV(GlimmerReference, GlimmerAccumulatedTexture) Texture2D<float4> glimmerAccumulated;

DECLARE_UAV(GlimmerReference, OutPartials) RWStructuredBuffer<float> OutPartials;

#define GLIMMER_REFERENCE_MAX_WAVES 16u

groupshared float gsWaveSums[GLIMMER_REFERENCE_MAX_WAVES][GLIMMER_REFERENCE_STATS];

// mean, and the variance of that mean, of a luminance from its sum and sum of squares over the frames
float2 GlimmerReferenceMeanAndVariance(float sum, float sumOfSquares, float frames)
{
    const float mean = sum / frames;
    const float variance = frames > 1.0 ? max(sumOfSquares / frames - mean * mean, 0.0) / (frames - 1.0) : 0.0;

    return float2(mean, variance);
}

[numthreads(GLIMMER_REFERENCE_STATS_GROUP, GLIMMER_REFERENCE_STATS_GROUP, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const int2 pixel = int2(dispatchThreadId.xy);

    float values[GLIMMER_REFERENCE_CLASS_VALUES] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    uint classIndex = GLIMMER_REFERENCE_CLASSES;

    if (all(dispatchThreadId.xy < constants.dimensions.xy) && all(dispatchThreadId.xy % max(constants.stats.z, 1u) == 0u))
    {
        const GlimmerReferencePixel surface = GlimmerReferenceLoadPixel(pixel);

        if (surface.isValid)
        {
            const float frames = float(max(constants.dimensions.w, 1u));

            const float4 referenceSum = glimmerReference.Load(int3(pixel, 0));
            const float4 glimmerSum = glimmerAccumulated.Load(int3(pixel, 0));

            const float3 reference = referenceSum.rgb / frames;
            const float3 glimmer = glimmerSum.rgb / frames;

            const float2 referenceLuminance = GlimmerReferenceMeanAndVariance(GlimmerReferenceLuminance(referenceSum.rgb), referenceSum.a, frames);
            const float2 glimmerLuminance = GlimmerReferenceMeanAndVariance(GlimmerReferenceLuminance(glimmerSum.rgb), glimmerSum.a, frames);

            const float luminanceError = glimmerLuminance.x - referenceLuminance.x;
            const float3 colorError = glimmer - reference;

            values[0] = 1.0;
            values[1] = referenceLuminance.x;
            values[2] = glimmerLuminance.x;
            values[3] = luminanceError * luminanceError;
            values[4] = referenceLuminance.y + glimmerLuminance.y;
            values[5] = dot(colorError, colorError) / 3.0;

            classIndex = surface.classIndex;
        }
    }

    const uint waveIndex = groupIndex / WaveGetLaneCount();

    [unroll]
    for (uint classSlot = 0; classSlot < GLIMMER_REFERENCE_CLASSES; classSlot++)
    {
        const bool isMember = classIndex < GLIMMER_REFERENCE_CLASSES && (classSlot == GLIMMER_REFERENCE_CLASS_ALL || classSlot == classIndex);

        [unroll]
        for (uint value = 0; value < GLIMMER_REFERENCE_CLASS_VALUES; value++)
        {
            const float sum = WaveActiveSum(isMember ? values[value] : 0.0);

            if (WaveIsFirstLane() && waveIndex < GLIMMER_REFERENCE_MAX_WAVES)
            {
                gsWaveSums[waveIndex][classSlot * GLIMMER_REFERENCE_CLASS_VALUES + value] = sum;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    const uint groupFlatIndex = groupId.y * constants.stats.y + groupId.x;

    if (groupIndex < GLIMMER_REFERENCE_STATS_STRIDE)
    {
        const uint numWaves = min((GLIMMER_REFERENCE_STATS_GROUP * GLIMMER_REFERENCE_STATS_GROUP + WaveGetLaneCount() - 1u) / WaveGetLaneCount(), GLIMMER_REFERENCE_MAX_WAVES);

        float sum = 0.0;

        if (groupIndex < GLIMMER_REFERENCE_STATS)
        {
            for (uint wave = 0; wave < numWaves; wave++)
            {
                sum += gsWaveSums[wave][groupIndex];
            }
        }

        OutPartials[groupFlatIndex * GLIMMER_REFERENCE_STATS_STRIDE + groupIndex] = sum;
    }
}
#endif

#endif
