#include "include/Defines.hlsli"

PERMUTE(SKINNING);
PERMUTE(ALPHA_DISCARD);
PERMUTE(INSTANCING);
PERMUTE(SHADING_TYPE, DEFERRED, FORWARD, LIGHTMAPPED, UNLIT);

// Used in FORWARD_CLUSTERED only
PERMUTE(FORWARD_CLUSTERED);
PERMUTE(RT_GI);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

PERMUTE(FORWARD_SHADING);
STATIC(MAX_LIGHTS, 4)

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float4 color : TEXCOORD2;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
    bool is_front_face : SV_IsFrontFace;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#define HAS_REFRACTION 1

#include "include/Material.hlsli"

#include "include/Scene.hlsli"
#include "include/Packing.hlsli"
#include "include/EnvProbes.hlsli"
#include "include/Gbuffer.hlsli"
#include "include/Entity.hlsli"
#include "include/Noise.hlsli"

DECLARE_SRV(Default, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
    float4x4 vpMatrix;
};

#ifdef SHADING_TYPE_FORWARD

DECLARE_SRV(Default, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
DECLARE_SRV(Default, EnvProbesDepthTexture) TextureCubeArray<float2> envProbesDepthTexture;

DECLARE_SRV(Default, GBufferMipChain) Texture2D GBufferMipChain;

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "include/BRDF.hlsli"
#include "include/FoliageLighting.hlsli"

#ifdef FORWARD_CLUSTERED

DECLARE_SRV(Default, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;
DECLARE_SRV(Default, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(Default, ClusterGridBuffer) ByteAddressBuffer ClusterGridBuffer;
DECLARE_SRV(Default, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

#include "deferred/ClusteredShading.hlsli"

#include "include/SkyVisibility.hlsli"

DECLARE_BUFFER_DYNAMIC(Default, ForwardIndirectConstants) cbuffer ForwardIndirectConstants
{
    EnvProbe skyProbe;
    SkyVisibilityCapture skyVisibilityCapture;
};

DECLARE_SRV(Default, SkyVisibilityTexture) Texture2D SkyVisibilityTexture;

#ifdef RT_GI
DECLARE_SRV(Default, DDGIIrradianceTexture) Texture2D probe_irradiance;
DECLARE_SRV(Default, DDGIDepthTexture) Texture2D probe_depth;

#include "include/RayTracing/GlobalIllumination/ProbeUniforms.hlsli"

DECLARE_BUFFER(Default, DDGIConstants) cbuffer DDGI
{
    DDGIConstants ddgiConstants;
};

#include "include/RayTracing/GlobalIllumination/SampleDDGI.hlsli"
#endif // RT_GI

#define DEFERRED_LIGHTING_HAS_SKY
#endif // FORWARD_CLUSTERED

#include "deferred/DeferredLighting.hlsli"

#undef DEFERRED_LIGHTING_HAS_SKY

#include "include/Shadows.hlsli"

#ifdef FORWARD_SHADING

DECLARE_BUFFER_DYNAMIC(Default, ForwardShadingConstants) cbuffer ForwardShadingConstants
{
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
    EnvProbe fallbackProbe; // always the scene's sky probe, or a zeroed EnvProbe if none

    // CSM of the first directional light (matches DirectionalLightCSMData)
    float4x4 shadowViewMat;

    float4 atlasU;
    float4 atlasV;
    float4 atlasScaleX;
    float4 atlasScaleY;

    uint4 atlasSlice;

    float4 cascadeScaleX;
    float4 cascadeScaleY;
    float4 cascadeScaleZ;

    float4 cascadeOffsetX;
    float4 cascadeOffsetY;
    float4 cascadeOffsetZ;

    uint numBoundLights;
    uint directionalCSMLightIndex; // ~0u if none
};

// cascades follow the main camera, so a point outside all of them is treated as lit
float GetDirectionalCSMShadow(float3 position, float3 N, float NdotL)
{
    float4 positionLS = mul(shadowViewMat, float4(position, 1.0));
    positionLS /= positionLS.w;

    const float4 uvX = positionLS.x * cascadeScaleX + cascadeOffsetX;
    const float4 uvY = positionLS.y * cascadeScaleY + cascadeOffsetY;
    const float4 uvZ = positionLS.z * cascadeScaleZ + cascadeOffsetZ;

    const float4 maxDist = max(abs(uvX - 0.5), max(abs(uvY - 0.5), abs(uvZ - 0.5)));
    const float4 insideMask = step(maxDist, (float4)0.5);

    [branch]
    if (dot(insideMask, (float4)1.0) < 0.5)
    {
        return 1.0;
    }

    int cascadeIndex = 3;
    cascadeIndex = (insideMask.z > 0.5) ? 2 : cascadeIndex;
    cascadeIndex = (insideMask.y > 0.5) ? 1 : cascadeIndex;
    cascadeIndex = (insideMask.x > 0.5) ? 0 : cascadeIndex;

    const float cascadeWidth = 1.0 / max(abs(cascadeScaleX[cascadeIndex]), 0.000001);
    const float normalOffset = GetCascadeNormalOffset(cascadeWidth, NdotL);

    float4 offsetPositionLS = mul(shadowViewMat, float4(position + N * normalOffset, 1.0));
    offsetPositionLS /= offsetPositionLS.w;

    float4 shadowMapCoord;
    shadowMapCoord.x = offsetPositionLS.x * cascadeScaleX[cascadeIndex] + cascadeOffsetX[cascadeIndex];
    shadowMapCoord.y = offsetPositionLS.y * cascadeScaleY[cascadeIndex] + cascadeOffsetY[cascadeIndex];
    shadowMapCoord.z = offsetPositionLS.z * cascadeScaleZ[cascadeIndex] + cascadeOffsetZ[cascadeIndex];
    shadowMapCoord.w = (float)atlasSlice[cascadeIndex];

    return GetShadowCSM(shadowMapCoord,
        float2(atlasU[cascadeIndex], atlasV[cascadeIndex]),
        float2(atlasScaleX[cascadeIndex], atlasScaleY[cascadeIndex]));
}

#endif // FORWARD_SHADING

#endif // SHADING_TYPE_FORWARD

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif // CURRENT_MATERIAL

#include "include/Parallax.hlsli"

// @TODO!!! Replace with vertex tangents.
bool ComputeUVTangentFrame(float3 N, float3 P, float2 uv, out float3 tangent, out float3 bitangent)
{
    const float3 dpdx = ddx(P);
    const float3 dpdy = ddy(P);
    const float2 duvdx = ddx(uv);
    const float2 duvdy = ddy(uv);

    const float3 dpdyPerp = cross(dpdy, N);
    const float3 dpdxPerp = cross(N, dpdx);
    
    const float handedness = dot(dpdx, dpdyPerp) < 0.0 ? -1.0 : 1.0;

    tangent = (dpdyPerp * duvdx.x + dpdxPerp * duvdy.x) * handedness;
    bitangent = (dpdyPerp * duvdx.y + dpdxPerp * duvdy.y) * -handedness;

    const float tangentLengthSq = dot(tangent, tangent);
    const float bitangentLengthSq = dot(bitangent, bitangent);

    if (min(tangentLengthSq, bitangentLengthSq) <= 1e-12 * max(tangentLengthSq, bitangentLengthSq))
    {
        tangent = (float3)0.0;
        bitangent = (float3)0.0;

        return false;
    }

    tangent *= rsqrt(tangentLengthSq);
    bitangent *= rsqrt(bitangentLengthSq);

    return true;
}

#ifdef ALPHA_DISCARD
#define ALPHA_CUTOUT_HARD_MIP 1.3

bool ShouldDiscardCutout(float alpha, float alphaThreshold, float mipLevel, float2 pixelPosition)
{
    const float softCoverage = saturate((alpha - alphaThreshold) / max(fwidth(alpha), 1e-4) + 0.5);
    const float hardCoverage = step(alphaThreshold, alpha);
    const float coverage = lerp(softCoverage, hardCoverage, saturate((mipLevel - ALPHA_CUTOUT_HARD_MIP) * 0.5));

    return coverage <= InterleavedGradientNoiseAnimated(pixelPosition, world_shader_data.frame_counter % 64u);
}
#endif // ALPHA_DISCARD

// #define DEBUG_RAW_REFLECTIONS

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const bool isFoliage = (input.object_mask & OBJECT_MASK_FOLIAGE) != 0;

    /// @TODO: Move to new Foliage custom shader!!!
    if (isFoliage)
    {
        const float3 faceCross = cross(ddx(input.position.xyz), ddy(input.position.xyz));
        const float faceCrossLengthSq = dot(faceCross, faceCross);

        if (!input.is_front_face)
        {
            const float normalBlend = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_FOLIAGE_NORMAL_BLEND);
            const float backfaceVolume = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_FOLIAGE_BACKFACE_VOLUME);

            const float3 vertexNormal = normalize(input.normal);
            float3 turnedNormal = -vertexNormal;

            if (backfaceVolume > 0.0 && faceCrossLengthSq > 1e-20)
            {
                const float3 faceNormal = faceCross * rsqrt(faceCrossLengthSq);
                const float3 keptNormal = normalize(vertexNormal - 2.0 * (1.0 - normalBlend) * dot(vertexNormal, faceNormal) * faceNormal);

                const float3 blendedNormal = lerp(-vertexNormal, keptNormal, backfaceVolume);
                const float blendedLengthSq = dot(blendedNormal, blendedNormal);

                turnedNormal = blendedLengthSq > 1e-8 ? blendedNormal * rsqrt(blendedLengthSq) : keptNormal;
            }

            input.normal = turnedNormal;
            input.bitangent = -input.bitangent;
        }
    }

    float3x3 tbn_matrix = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));

    const float3 P = input.position.xyz;

    const float3 V = normalize(camera.position.xyz - P);
    float3 N = normalize(input.normal);

    output.gbuffer_albedo = CURRENT_MATERIAL.albedo;

    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);
    const float alpha_threshold = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ALPHA_THRESHOLD);

    float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;

    // derivatives need uniform control flow, so this goes before any branch or clip. degenerate UVs keep the old basis
    float3 uvTangent;
    float3 uvBitangent;

    if (ComputeUVTangentFrame(N, P, texcoord, uvTangent, uvBitangent))
    {
        // flipped foliage back faces mirror the whole perturbation, like the underside of a thin sheet
        const float backFaceSign = (isFoliage && !input.is_front_face) ? -1.0 : 1.0;

        tbn_matrix = float3x3(uvTangent * backFaceSign, uvBitangent * backFaceSign, N);
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, ParallaxMap))
    {
        bool flipHeight = bool(GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_PARALLAX_INVERSE_HEIGHT));

        float3x3 cotangent_matrix = CotangentFrame(N, P, texcoord);
        float3 tangent_view = mul(cotangent_matrix, V);
        float2 parallax_texcoord = ParallaxMappedTexCoords(
            CURRENT_MATERIAL.parallax_height,
            texcoord,
            normalize(tangent_view),
            flipHeight);

        texcoord = parallax_texcoord;
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

#ifdef ALPHA_DISCARD
        const float diffuseMipLevel = GET_TEXTURE(CURRENT_MATERIAL, DiffuseMap).CalculateLevelOfDetail(texture_sampler, texcoord);

        if (ShouldDiscardCutout(albedo_texture.a, alpha_threshold, diffuseMipLevel, input.position_cs.xy))
        {
            discard;
        }
#endif
        output.gbuffer_albedo *= albedo_texture;
    }

    output.gbuffer_albedo.a = max(output.gbuffer_albedo.a, 0.005);

    float4 normals_texture = float4(0.0, 0.0, 0.0, 0.0);

 #ifndef DEBUG_RAW_REFLECTIONS
    if (HAS_TEXTURE(CURRENT_MATERIAL, NormalMap))
    {
        normals_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, NormalMap, texcoord) * 2.0 - 1.0;
        normals_texture.y *= select((float) GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_NORMAL_MAP_FLIP_Y), -1.0f, 1.0f);

        N = normalize(mul(normals_texture.xyz, tbn_matrix));
    }
 #endif

    if (HAS_TEXTURE(CURRENT_MATERIAL, MetalnessMap))
    {
        float4 metalness_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, MetalnessMap, texcoord);
        float metalness_sample = SelectMaterialChannel(metalness_texture, GET_MATERIAL_METALNESS_CHANNEL(CURRENT_MATERIAL));

        metalness = metalness_sample;
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, RoughnessMap))
    {
        float4 roughness_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, RoughnessMap, texcoord);
        float roughness_sample = SelectMaterialChannel(roughness_texture, GET_MATERIAL_ROUGHNESS_CHANNEL(CURRENT_MATERIAL));

        roughness = roughness_sample;
    }

    // roughness is authored as perceptual roughness; need to convert to physical roughness for the BRDF calculations
    const float perceptualRoughness = roughness;
    roughness = roughness * roughness;

#if SHADING_TYPE_FORWARD
    float3 albedo = output.gbuffer_albedo.rgb;

    const bool drawUnlit = (input.object_mask & OBJECT_MASK_UNLIT) != 0;

    if (drawUnlit)
    {
        output.gbuffer_albedo = float4(albedo, 1.0);
    }
    else
    {
        float3 diffuseColor = CalculateDiffuseColor(albedo, metalness);

        // transmission on foliage == light going through the leaf. not refraction
        const float refractionTransmission = isFoliage ? 0.0 : transmission;
        const float foliageTransmission = isFoliage ? transmission : 0.0;

        {
            float3 indirect_lighting = 0;
            float3 direct_lighting = 0;

            const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));

            const float3 R = normalize(reflect(-V, N));

            const float3 F0 = CalculateF0(albedo, metalness);
            const float3 dfg = CalculateDFG(perceptualRoughness, NdotV);
            const float3 E = CalculateE(F0, dfg);

            const float3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

#ifdef FORWARD_CLUSTERED
            // this pixel's screen-space UV, needed to index the cluster grid (input.texcoord0 is the mesh's own UV0, not screen position)
            const float2 screenUV = (input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5;

            float4 positionVS = mul(camera.view, float4(P, 1.0));
            positionVS /= positionVS.w;
#endif // FORWARD_CLUSTERED

            { // Indirect part.
                float3 Ft = CalculateRefraction(
                    camera.dimensions.xy,
                    P, N, V,
                    texcoord,
                    F0, E,
                    refractionTransmission, perceptualRoughness,
                    float4(0.0, 0.0, 0.0, 0.0),
                    output.gbuffer_albedo,
                    float3(ao, ao, ao));

                float3 Fr = (float3)0;
                float3 Fd = (float3)0;

#ifdef FORWARD_CLUSTERED
                {
                    float4 reflections = (float4)0;
                    float4 irradiance = (float4)0;

                    g_skyVisibility = EvaluateSkyVisibility(skyVisibilityCapture, SkyVisibilityTexture, P, N, input.position_cs.xy - 0.5);

                    EvaluateEnvProbes(
                        positionVS.xyz, P,
                        N, V, R,
                        camera.near, camera.far,
                        roughness, perceptualRoughness,
                        screenUV, camera.dimensions.xy,
                        input.object_mask,
                        /* inout */ reflections,
                        /* inout */ irradiance);

                    reflections.a = saturate(reflections.a);
                    irradiance.a = saturate(irradiance.a);

#ifdef RT_GI
                    // ddgi alpha fades out past the last cascade, falling back to probe and sky irradiance
                    const float4 ddgiIrradiance = DDGISampleIrradiance(P, N, V);
                    irradiance = lerp(irradiance, ddgiIrradiance, ddgiIrradiance.a);
#endif // RT_GI

                    Fd = diffuseColor * irradiance.rgb * (1.0 - E) * ao;

                    const float3 specular_ao = (float3) SpecularAO_Lagarde(NdotV, ao, perceptualRoughness);
                    Fr = E * (reflections.rgb * specular_ao * energy_compensation) * reflections.a;
                }
#endif // FORWARD_CLUSTERED

                Ft *= refractionTransmission;
                Fd *= (1.0 - refractionTransmission);

                indirect_lighting = Ft + Fd + Fr;
            }

    #ifdef FORWARD_CLUSTERED
            const uint2 pixelCoord = uint2(screenUV * max(0, int2(camera.dimensions.xy) - 1));

            const float viewSpaceZ = positionVS.z;

            // Clustered shading
            const uint gridIndex = Cluster_GetGridIndex(
                camera.dimensions.xy, pixelCoord,
                viewSpaceZ,
                camera.near, camera.far);

            const uint2 clusterData = ClusterGridBuffer.Load2(gridIndex * sizeof(uint2));

            const uint clusterIndexOffset = clusterData.x;

            const uint numLights = (clusterData.y & 0xFFFF);
            const uint numEnvProbes = (clusterData.y >> 16) & 0xFFFF;

            for (uint i = 0; i < numLights; ++i)
            {
                const uint lightIndex = Cluster_LoadLightIndex(clusterIndexOffset, i);

                Light currentLight = LightsBuffer.Load(lightIndex);

                float3 L = currentLight.position_intensity.xyz;
                L -= P * float(min(currentLight.type, 1));

                L = normalize(L);

                const float3 H = normalize(L + V);

                const float NdotL = max(0.000001, dot(N, L));
                const float LdotH = max(0.000001, dot(L, H));
                const float NdotH = max(0.000001, dot(N, H));
                const float HdotV = max(0.000001, dot(H, V));

                float3 light_color = currentLight.color.rgb;

                float attenuation = 1.0;
                float shadow = 1.0;

                const float D = CalculateDistributionTerm(perceptualRoughness, NdotH);
                const float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);
                const float3 F = CalculateFresnelTerm(F0, LdotH);

                const float3 specular_lobe = D * G * F;

                switch (currentLight.type)
                {
                    case HYP_LIGHT_TYPE_POINT:
                    case HYP_LIGHT_TYPE_SPOT: // fallthrough
                    {
                        const float2 radiusFalloff = float2(f16tof32(currentLight.radiusFalloffPacked), f16tof32(currentLight.radiusFalloffPacked >> 16));
                        const float radius = radiusFalloff.x;
                        const float falloff = radiusFalloff.y;

                        attenuation = GetSquareFalloffAttenuation(P, currentLight.position_intensity.xyz, radius);

                        if (currentLight.type == HYP_LIGHT_TYPE_SPOT)
                        {
                            float theta = max(dot(-L, normalize(currentLight.normal.xyz)), 0.0);
                            float2 spot_angles = currentLight.area_size.xy;

                            attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);

                            // @TODO Spot shadows - add here when adding to DeferredDirect.hlsl

                        }
                        else
                        {
                            // @TODO Forward clustered needs shadows.
                            if ((currentLight.flags & LF_SHADOW_CASTER) != 0)
                            {
                                //uint shadowMapIndex = GetShadowMapIndexForLight(lightIndex);
                                //ShadowMap shadowMap = shadowMaps[shadowMapIndex];

                                float3 worldToLight = P - currentLight.position_intensity.xyz;

                                //shadow = GetPointShadow(shadowMap, currentLight.flags, worldToLight, NdotL);
                            }
                        }

                        break;
                    }
                    default: break;
                }

                float3 specular = specular_lobe;

                float3 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;
                float3 diffuse = diffuse_lobe;

                const float diffuseWeight = isFoliage ? FoliageWrapDiffuse(dot(N, L)) : NdotL;

                float3 direct_component = diffuse * diffuseWeight + specular * energy_compensation * NdotL;

                if (isFoliage)
                {
                    direct_component += diffuse * FoliageTransmission(V, L, foliageTransmission);
                }

                direct_lighting += direct_component * (light_color * ao * shadow * currentLight.position_intensity.w * attenuation);
            }
    #endif // CLUSTERED

    #ifdef FORWARD_SHADING
            // Bounded light list from ForwardShadingConstants; only directional lights are consumed here,
            // point/spot are already covered (unbounded, tile-culled) by the cluster loop above.
            for (uint fsLightIdx = 0; fsLightIdx < min(numBoundLights, MAX_LIGHTS); fsLightIdx++)
            {
                Light currentLight = lights[fsLightIdx];

                if (currentLight.type != HYP_LIGHT_TYPE_DIRECTIONAL)
                {
                    continue;
                }

                const float3 L = normalize(currentLight.position_intensity.xyz);
                const float3 H = normalize(L + V);

                const float NdotL = max(0.000001, dot(N, L));
                const float LdotH = max(0.000001, dot(L, H));
                const float NdotH = max(0.000001, dot(N, H));

                float shadow = 1.0;

                if ((currentLight.flags & LF_SHADOW_CASTER) != 0 && fsLightIdx == directionalCSMLightIndex)
                {
                    shadow = GetDirectionalCSMShadow(P, N, NdotL);
                }

                const float D = CalculateDistributionTerm(perceptualRoughness, NdotH);
                const float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);
                const float3 F = CalculateFresnelTerm(F0, LdotH);

                const float3 specular_lobe = D * G * F;
                const float3 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;

                const float diffuseWeight = isFoliage ? FoliageWrapDiffuse(dot(N, L)) : NdotL;

                float3 direct_component = diffuse_lobe * diffuseWeight + specular_lobe * energy_compensation * NdotL;

                if (isFoliage)
                {
                    direct_component += diffuse_lobe * FoliageTransmission(V, L, foliageTransmission);
                }

                direct_lighting += direct_component * (currentLight.color.rgb * ao * shadow * currentLight.position_intensity.w);
            }
    #endif // FORWARD_SHADING

            output.gbuffer_albedo.rgb = indirect_lighting + direct_lighting;
        }
    }

    output.gbuffer_albedo.rgb += GET_MATERIAL_EMISSIVE(CURRENT_MATERIAL);

    // premultiplied and additive blends take the source as-is, so fade it by coverage here
    if (GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_PREMULTIPLIED_ALPHA))
    {
        output.gbuffer_albedo.rgb *= output.gbuffer_albedo.a;
    }
#endif // SHADING_TYPE_FORWARD

#ifndef SHADING_TYPE_FORWARD
    // gbuffer albedo alpha carries material ambient occlusion, which only attenuates indirect light.
    // The deferred path never consumed the opacity that was written here.
    output.gbuffer_albedo.a = ao;
#endif // !SHADING_TYPE_FORWARD

#ifdef DEBUG_RAW_REFLECTIONS
    roughness = 0.0001;
#endif

    // https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
    // see: "Motion Vectors" section
    // uv y runs opposite to ndc y, and consumers reproject with uv - velocity
    float2 velocity = ((input.position_ndc.xy / input.position_ndc.w) - (input.previous_position_ndc.xy / input.previous_position_ndc.w)) * float2(0.5, -0.5);

    uint mask = input.object_mask;

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = metalness;
    materialParams.mask = mask;

    output.gbuffer_normals = GBufferPackNormal(N);

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);

    output.gbuffer_normals.x = roughnessAndMetalPacked;

#ifdef SHADING_TYPE_LIGHTMAPPED
    // atlas UV, 14 bits per channel (0-16383)
    output.gbuffer_material = min((uint)round(saturate(input.texcoord1.x) * 16384.0), 16383u)
        | (min((uint)round(saturate(input.texcoord1.y) * 16384.0), 16383u) << 14u);
#else
    // foliage keeps its transmission amount in the low 8 bits
    output.gbuffer_material = isFoliage ? uint(saturate(transmission) * 255.0 + 0.5) : 0u;

#ifndef SHADING_TYPE_FORWARD
    output.gbuffer_material |= GBufferPackEmissive(GET_MATERIAL_EMISSIVE(CURRENT_MATERIAL));
#endif // !SHADING_TYPE_FORWARD
#endif

    // Mask is stored in the upper 4 bits of gbuffer_material
    output.gbuffer_material |= (maskPacked << 28u);

    output.gbuffer_velocity = velocity;

    return output;
}
