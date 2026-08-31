#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

STATIC(MAX_LIGHTS, 4)

// Match MaxLightmapVolumeAssignment
STATIC(MAX_LIGHTMAP_VOLUMES, 4)

PERMUTE(MODE_SHADOWS)
PERMUTE(WRITE_NORMALS)
PERMUTE(WRITE_MOMENTS)
PERMUTE(WRITE_HIT_MASK)
PERMUTE(FORWARD_SHADING)
PERMUTE(APPLY_LIGHTMAPS)
PERMUTE(INSTANCING)
PERMUTE(SKINNING)

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE_OPTIONAL float3 a_normal : NORMAL;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL uint a_bone_indices : BLENDINDICES;
    HYP_ATTRIBUTE_OPTIONAL float4 a_bone_weights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
};

#include "include/Entity.hlsli"
#include "include/Material.hlsli"

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING
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

#ifdef SKINNING
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<float4x4> SkeletonsBuffer;
#include "include/Skinning.hlsli"
#endif // SKINNING

#include "include/Instancing.hlsli"

float4x4 LookAt(float3 pos, float3 target, float3 up)
{
    float3 forward = normalize(target - pos);
    float3 right = normalize(cross(up, forward));
    float3 newUp = cross(forward, right);

    return float4x4(
        right.x, right.y, right.z, -dot(right, pos),
        newUp.x, newUp.y, newUp.z, -dot(newUp, pos),
        forward.x, forward.y, forward.z, -dot(forward, pos),
        0.0, 0.0, 0.0, 1.0
    );
}

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4 position;

#ifdef INSTANCING
    uint entityIndex;
    uint dataOffset;
    LoadEntityIndexAndDataOffset(instanceId, entityIndex, dataOffset);

    float4x4 transform = LoadInstanceTransform(s_offsetOfTransforms + (sizeof(float4x4) * dataOffset));

    output.object_index = entityIndex;

    Entity currentEntity = entities[entityIndex];
    float4x4 model_matrix = mul(currentEntity.model_matrix, transform);
    float3x3 normal_matrix = (float3x3)currentEntity.normal_matrix;
#else   // !INSTANCING
    Entity currentEntity = entity;
    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = (float3x3)entity.normal_matrix;

    output.object_index = ~0u; // unused
#endif  // INSTANCING

#if defined(SKINNING) && defined(VT_Skeletal)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    normal_matrix = mul(normal_matrix, (float3x3)skinning_matrix);
#else   // !SKINNING
    position = mul(model_matrix, float4(input.a_position, 1.0));
#endif  // SKINNING

    output.position = position.xyz / position.w;

#ifdef HYP_ATTRIBUTE_a_normal
    output.normal = mul(normal_matrix, input.a_normal);
#else // !HYP_ATTRIBUTE_a_normal
    output.normal = (float3)0;
#endif // HYP_ATTRIBUTE_a_normal
    
#ifdef HYP_ATTRIBUTE_a_texcoord0
    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
#else
    output.texcoord0 = float2(0.0, 0.0);
#endif // HYP_ATTRIBUTE_a_texcoord0

#ifdef HYP_ATTRIBUTE_a_texcoord1
    output.texcoord1 = float2(input.a_texcoord1.x, input.a_texcoord1.y);
#else
    output.texcoord1 = float2(0.0, 0.0);
#endif // HYP_ATTRIBUTE_a_texcoord1

    output.camera_position = camera.position.xyz;

    output.position_cs = mul(vpMatrix, position);

    return output;
}

#endif

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
};

struct PSOutput
{
#ifdef MODE_SHADOWS
    #ifdef WRITE_MOMENTS // For variant shadow map
        float2 output_moments : SV_Target0;
    #endif // WRITE_MOMENTS
#else // !MODE_SHADOWS
    float4 output_color : SV_Target0;

    #ifdef WRITE_NORMALS
        float2 output_normals : SV_Target1;
        #ifdef WRITE_MOMENTS
            float2 output_moments : SV_Target2;
            #ifdef WRITE_HIT_MASK
                float output_hit_mask : SV_Target3;
            #endif // WRITE_HIT_MASK
        #else // !WRITE_MOMENTS
            #ifdef WRITE_HIT_MASK
                float output_hit_mask : SV_Target2;
            #endif // WRITE_HIT_MASK
        #endif // WRITE_MOMENTS
    #else // !WRITE_NORMALS
        #ifdef WRITE_MOMENTS
            float2 output_moments : SV_Target1;
            #ifdef WRITE_HIT_MASK
                float output_hit_mask : SV_Target2;
            #endif // WRITE_HIT_MASK
        #else // !WRITE_MOMENTS
            #ifdef WRITE_HIT_MASK
                float output_hit_mask : SV_Target1;
            #endif // WRITE_HIT_MASK
        #endif // WRITE_MOMENTS
    #endif // WRITE_NORMALS
#endif // MODE_SHADOWS
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#include "include/Material.hlsli"
#include "include/Gbuffer.hlsli"
#include "include/EnvProbes.hlsli"
#include "include/Octahedron.hlsli"
#include "include/Entity.hlsli"
#include "include/Packing.hlsli"
#include "include/BRDF.hlsli"

#define HYP_CUBEMAP_AMBIENT 0.005

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
#endif // INSTANCING

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // INSTANCING
    Camera camera;
    Material material;
};

#ifdef FORWARD_SHADING

struct LightmapVolumeData
{
    float4 aabbMin;
    float4 aabbMax;
};

DECLARE_BUFFER_DYNAMIC(Default, ForwardShadingConstants) cbuffer ForwardShadingConstants
{
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
    EnvProbe fallbackProbe; // always the scene's sky probe, or a zeroed EnvProbe if none
    
    uint numBoundLights;
};

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "include/Shadows.hlsli"

#endif // FORWARD_SHADING

#ifdef APPLY_LIGHTMAPS

DECLARE_BUFFER_DYNAMIC(Default, ApplyLightmapsConstants) cbuffer ApplyLightmapsConstants
{
    LightmapVolumeData lightmapVolumes[MAX_LIGHTMAP_VOLUMES];
    uint numLightmapVolumes;
};

DECLARE_SRV(Default, LightmapVolumeIrradianceTexture0) Texture2D LightmapVolumeIrradianceTexture0;
DECLARE_SRV(Default, LightmapVolumeIrradianceTexture1) Texture2D LightmapVolumeIrradianceTexture1;
DECLARE_SRV(Default, LightmapVolumeIrradianceTexture2) Texture2D LightmapVolumeIrradianceTexture2;
DECLARE_SRV(Default, LightmapVolumeIrradianceTexture3) Texture2D LightmapVolumeIrradianceTexture3;

#endif // APPLY_LIGHTMAPS

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float3 vsPosition = input.camera_position - input.position;

    const float3 V = normalize(vsPosition);
    const float3 N = normalize(input.normal);

    float4 albedo = float4(1.0, 1.0, 1.0, 1.0);

    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;
        albedo = CURRENT_MATERIAL.albedo;

        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

        clip(albedo_texture.a - 0.2);

        albedo *= albedo_texture;
    }

    float metalness = 0.0;//GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);

#ifdef WRITE_MOMENTS
    const float dist = distance(input.position, input.camera_position) / max(camera.far, HYP_FMATH_EPSILON);
    float2 moments = float2(dist, HYP_FMATH_SQR(dist));

    float dx = ddx(dist);
    float dy = ddy(dist);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

#endif // WRITE_MOMENTS

#ifndef MODE_SHADOWS
#ifdef FORWARD_SHADING
    {
        const float3 diffuseColor = CalculateDiffuseColor(albedo.rgb, metalness);
        const float3 F0 = CalculateF0(albedo.rgb, metalness);

        const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
        float3 directLight = (float3)0;

        for (uint lightIdx = 0; lightIdx < min(numBoundLights, MAX_LIGHTS); lightIdx++)
        {
#define CURRENT_LIGHT (lights[lightIdx])

            const uint lightType = CURRENT_LIGHT.type;
            const uint lightFlags = CURRENT_LIGHT.flags;

            const float4 lightPositionIntensity = CURRENT_LIGHT.position_intensity;

            float3 L = lightPositionIntensity.xyz;
            L -= input.position * float(min(lightType, 1));
            L = normalize(L);

            const float3 H = normalize(L + V);

            const float NdotL = max(0.000001, dot(N, L));
            const float LdotH = max(0.000001, dot(L, H));
            const float NdotH = max(0.000001, dot(N, H));
            const float HdotV = max(0.000001, dot(H, V));

            float3 light_color = CURRENT_LIGHT.color.rgb;
            float attenuation = 1.0;
            float shadow = 1.0;

            if (lightType == HYP_LIGHT_TYPE_DIRECTIONAL)
            {
                // directional shadow
                if ((lightFlags & LF_SHADOW_CASTER) != 0)
                {
                    shadow = GetShadowStandard(shadowMaps[lightIdx], input.position, float2(0, 0), NdotL);
                }
            }
            else
            {
                const uint radiusFalloffPacked = CURRENT_LIGHT.radiusFalloffPacked;

                const float2 radiusFalloff = float2(
                    f16tof32(radiusFalloffPacked),
                    f16tof32(radiusFalloffPacked >> 16));

                const float radius = radiusFalloff.x;

                attenuation = GetSquareFalloffAttenuation(input.position, lightPositionIntensity.xyz, radius);

                if (lightType == HYP_LIGHT_TYPE_SPOT)
                {
                    const float theta = max(dot(-L, normalize(CURRENT_LIGHT.normal.xyz)), 0.0);
                    const float2 spot_angles = CURRENT_LIGHT.area_size.xy;

                    attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0]))
                        * step(spot_angles[0], theta);
                }
                
                if ((lightFlags & LF_SHADOW_CASTER) != 0)
                {
                    float3 worldToLight = input.position - lightPositionIntensity.xyz;

                    shadow = GetPointShadowStandard(shadowMaps[lightIdx], worldToLight, NdotL);
                }
            }

            const float3 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;

            directLight += (diffuse_lobe) * shadow * NdotL * lightPositionIntensity.w * attenuation * light_color;

#undef CURRENT_LIGHT
        }

        float3 indirectLight = (float3)0;
        bool hasLightmapContribution = false;

#ifdef APPLY_LIGHTMAPS
        // clang-format off
#define HYP_APPLY_LIGHTMAP_VOLUME(idx, tex)                                                       \
            if (!hasLightmapContribution && idx < numLightmapVolumes)                                     \
            {                                                                                              \
                LightmapVolumeData lmv = lightmapVolumes[idx];                                             \
                const float3 d = max(lmv.aabbMin.xyz - input.position, input.position - lmv.aabbMax.xyz);  \
                if (max(d.x, max(d.y, d.z)) <= 0.0)                                                        \
                {                                                                                           \
                    const float4 irradiance = SAMPLE_TEXTURE_2D_LOD(sampler_linear, tex, input.texcoord1, 0); \
                    indirectLight = diffuseColor * irradiance.rgb;                                         \
                    hasLightmapContribution = true;                                                        \
                }                                                                                            \
            }

       HYP_APPLY_LIGHTMAP_VOLUME(0, LightmapVolumeIrradianceTexture0)
       HYP_APPLY_LIGHTMAP_VOLUME(1, LightmapVolumeIrradianceTexture1)
       HYP_APPLY_LIGHTMAP_VOLUME(2, LightmapVolumeIrradianceTexture2)
       HYP_APPLY_LIGHTMAP_VOLUME(3, LightmapVolumeIrradianceTexture3)
        
#undef HYP_APPLY_LIGHTMAP_VOLUME
        // clang-format on

#endif // APPLY_LIGHTMAPS

        if (!hasLightmapContribution)
        {
            float shBands[9];
            ProjectSHBands(N, shBands);

            indirectLight = diffuseColor * EnvProbeSH(fallbackProbe, shBands);
        }

        output.output_color.rgb = saturate(directLight + indirectLight);
    }
#else
    output.output_color.rgb = albedo.rgb;
#endif
    output.output_color.a = 1.0;

#ifdef WRITE_NORMALS
    output.output_normals = PackNormalVec2(N);
#endif // WRITE_NORMALS

#endif // !MODE_SHADOWS

#ifdef WRITE_MOMENTS
    output.output_moments = moments;
#endif // WRITE_MOMENTS

#ifdef WRITE_HIT_MASK
    output.output_hit_mask = 1.0;
#endif // WRITE_HIT_MASK

    return output;
}

#endif // PIXEL_SHADER
