#include "include/Defines.hlsli"

PERMUTE(SKINNING);
PERMUTE(ALPHA_DISCARD);
PERMUTE(INSTANCING);
PERMUTE(SHADING_TYPE, DEFERRED, FORWARD, LIGHTMAPPED, UNLIT);

// Used in FORWARD_CLUSTERED only
PERMUTE(FORWARD_CLUSTERED);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

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
    float4 shData[9];
};

#define HAS_PROBE_LIGHTING (step(0.5, shData[0].w))

float3 EvaluateSH(float3 N)
{
    float bands[9];
    ProjectSHBands(N, bands);

    float3 result = (float3)0.0;

    for (int i = 0; i < 9; i++)
    {
        result += shData[i].rgb * bands[i];
    }

    return max(result, (float3)0.0);
}

#ifdef SHADING_TYPE_FORWARD

DECLARE_SRV(Default, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
DECLARE_SRV(Default, EnvProbesDepthTexture) TextureCubeArray<float2> envProbesDepthTexture;

DECLARE_SRV(Default, GBufferMipChain) Texture2D GBufferMipChain;

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "include/BRDF.hlsli"

#ifdef FORWARD_CLUSTERED

DECLARE_SRV(Default, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;
DECLARE_SRV(Default, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(Default, ClusterGridBuffer) ByteAddressBuffer ClusterGridBuffer;
DECLARE_SRV(Default, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

#include "deferred/ClusteredShading.hlsli"
#endif // FORWARD_CLUSTERED

#include "deferred/DeferredLighting.hlsli"
#include "include/Shadows.hlsli"
#endif // SHADING_TYPE_FORWARD

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif // CURRENT_MATERIAL

#include "include/Parallax.hlsli"

// #define DEBUG_RAW_REFLECTIONS

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3x3 tbn_matrix = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
    float3 view_vector = normalize(input.camera_position - input.position);

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

    if (HAS_TEXTURE(CURRENT_MATERIAL, ParallaxMap))
    {
        float3 tangent_view = mul(tbn_matrix, view_vector);
        float2 parallax_texcoord = ParallaxMappedTexCoords(
            CURRENT_MATERIAL.parallax_height,
            texcoord,
            normalize(tangent_view));

        texcoord = parallax_texcoord;
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

#ifdef ALPHA_DISCARD
        clip(albedo_texture.a - alpha_threshold);
#endif
        output.gbuffer_albedo *= albedo_texture;
    }

    output.gbuffer_albedo.a = max(output.gbuffer_albedo.a, 0.005);

    float4 normals_texture = float4(0.0, 0.0, 0.0, 0.0);

 #ifndef DEBUG_RAW_REFLECTIONS
    if (HAS_TEXTURE(CURRENT_MATERIAL, NormalMap))
    {
        normals_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, NormalMap, texcoord) * 2.0 - 1.0;

        if (GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_NORMAL_MAP_FLIP_Y))
        {
            normals_texture.y = -normals_texture.y;
        }

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

        {
            float3 indirect_lighting = 0;
            float3 direct_lighting = 0;

            const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));

            const float3 R = normalize(reflect(-V, N));

            const float3 F0 = CalculateF0(albedo, metalness);
            const float3 dfg = CalculateDFG(perceptualRoughness, NdotV);
            const float3 E = CalculateE(F0, dfg);

            const float3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

            { // Indirect part.
                float4 irradiance = (float4)0;
                float4 reflections = (float4)0;

                float3 Ft = CalculateRefraction(
                    camera.dimensions.xy,
                    P, N, V,
                    texcoord,
                    F0, E,
                    transmission, perceptualRoughness,
                    float4(0.0, 0.0, 0.0, 0.0),
                    output.gbuffer_albedo,
                    float3(ao, ao, ao));

                // @TODO
                // Select env probes - add reflection + irradiance using EvaluateEnvProbes
                // Calc Fr + Fd
                // Bada bing badaboom

                float3 Fr = (float3)0;
                float3 Fd = (float3)0;

                // @TODO Fd and Fr.

                Ft *= transmission;
                Fd *= (1.0 - transmission);

                indirect_lighting = Ft + Fd + Fr;
            }

    #ifdef FORWARD_CLUSTERED
            const uint2 pixelCoord = uint2(input.texcoord0 * max(0, int2(camera.dimensions.xy) - 1));

            // @TODO!!! This is poopy; just reconstruct view space position in the shader and use that for cluster indexing instead of reconstructing world space position and then transforming to view space
            float4 positionVS = mul(camera.view, float4(P, 1.0));
            positionVS /= positionVS.w;

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

                float3 direct_component = diffuse + specular * energy_compensation;

                direct_lighting += (direct_component * (light_color * ao * NdotL * shadow * currentLight.position_intensity.w * attenuation)).rgb;
            }
    #endif // CLUSTERED

            output.gbuffer_albedo.rgb = indirect_lighting + direct_lighting;
        }
    }
#endif // SHADING_TYPE_FORWARD

#ifdef DEBUG_RAW_REFLECTIONS
    roughness = 0.3;
#endif

    // https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
    // see: "Motion Vectors" section
    float2 velocity = float2(((input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5) - ((input.previous_position_ndc.xy / input.previous_position_ndc.w) * 0.5 + 0.5));

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
    // 14 bits per channel (0-16383)
    output.gbuffer_material = ((uint)round(input.texcoord1.x * 16384.0) & 0x3FFFu)
        | (((uint)round(input.texcoord1.y * 16384.0) & 0x3FFFu) << 14u);
#else
    //Probe lighting - evaluate SH, store RGB8 in the upper 24 bits of gbuffer_material
    if (HAS_PROBE_LIGHTING)
    {
        float3 shResult = EvaluateSH(N);
        output.gbuffer_material = ((uint)(shResult.x * 255.0) & 0xFFu)
            | (((uint)(shResult.y * 255.0) & 0xFFu) << 8u)
            | (((uint)(shResult.z * 255.0) & 0xFFu) << 16u);
    }
    else
    {
        output.gbuffer_material = 0;
    }
#endif

    // Mask is stored in the upper 4 bits of gbuffer_material
    output.gbuffer_material |= (maskPacked << 28u);

    output.gbuffer_velocity = velocity;

    return output;
}
