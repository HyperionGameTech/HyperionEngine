#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

STATIC(VIEW_COUNT, 6)
STATIC(MAX_LIGHTS, 4)

PERMUTE(WRITE_NORMALS)
PERMUTE(WRITE_MOMENTS)

PERMUTE(FORWARD_SHADING)

PERMUTE(INSTANCING)
PERMUTE(SKINNING)

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
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
    MeshEntityInstanceBatch batch = EntityInstanceBatchBuffer.Load<MeshEntityInstanceBatch>(0);

    Entity currentEntity = entities[batch.indices[instanceId / 4][instanceId % 4]];
    float4x4 model_matrix = mul(batch.transforms[instanceId], currentEntity.model_matrix);
    float3x3 normal_matrix = (float3x3)currentEntity.normal_matrix;
#else
    Entity currentEntity = entity;
    float4x4 model_matrix = entity.model_matrix;
    float3x3 normal_matrix = (float3x3)entity.normal_matrix;
#endif

#if defined(SKINNING) && defined(VT_Skeletal)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);

    position = mul(model_matrix, mul(skinning_matrix, float4(input.a_position, 1.0)));
    normal_matrix = mul(normal_matrix, (float3x3)skinning_matrix);
#else
    position = mul(model_matrix, float4(input.a_position, 1.0));
#endif

    output.position = position.xyz / position.w;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = float2(input.a_texcoord0.x, 1.0 - input.a_texcoord0.y);
    output.camera_position = camera.position.xyz;

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#endif

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
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
};

struct PSOutput
{
float4 output_color : SV_Target0;

#ifdef WRITE_NORMALS
float2 output_normals : SV_Target1;
#ifdef WRITE_MOMENTS
float2 output_moments : SV_Target2;
#endif // !WRITE_MOMENTS
#else // !WRITE_NORMALS
#ifdef WRITE_MOMENTS
float2 output_moments : SV_Target1;
#endif // WRITE_MOMENTS
#endif // WRITE_NORMALS
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

DECLARE_BUFFER_DYNAMIC(Default, ForwardShadingConstants) cbuffer ForwardShadingConstants
{
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
    EnvProbe fallbackProbe;
    uint numBoundLights;
};

DECLARE_SRV(Default, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "include/Shadows.hlsli"

#endif // FORWARD_SHADING

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float3 vsPosition = input.camera_position - input.position;

    const float3 V = normalize(vsPosition);
    const float3 N = normalize(input.normal);
    const float3 R = reflect(-V, N);

    float4 albedo = float4(1.0, 1.0, 1.0, 1.0);

    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;
        albedo = CURRENT_MATERIAL.albedo;

        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

        clip(albedo_texture.a - 0.2);

        albedo *= albedo_texture;
    }

    float roughness = 1.0;//GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float metalness = 0.0;//GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float perceptualRoughness = roughness;
    roughness = roughness * roughness;

#ifdef WRITE_MOMENTS
    const float dist = length(input.position - input.camera_position);
    float2 moments = float2(dist, HYP_FMATH_SQR(dist));

// #ifdef MODE_SHADOWS
//     float dx = ddx(dist);
//     float dy = ddy(dist);

//     moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));
// #endif // MODE_SHADOWS

#endif // WRITE_MOMENTS

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

            const float3 diffuse_lobe = diffuseColor;// * HYP_FMATH_ONE_OVER_PI;

            directLight += (diffuse_lobe) * shadow;// * NdotL * lightPositionIntensity.w * attenuation * light_color;

#undef CURRENT_LIGHT
        }

        float3 indirectLight = (float3)0;

        const uint fallbackTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(fallbackProbe);

        if (fallbackTextureIndex != INVALID_ENV_PROBE_TEXTURE)
        {
            float4 reflections = (float4)0;

            const float3 aabbMin = fallbackProbe.aabb_min.xyz;
            const float3 aabbMax = fallbackProbe.aabb_max.xyz;

            const float numMips = 7.0; // assuming 128x128 cubemap size for reflection probes
            const float lod = perceptualRoughness * numMips;

            reflections = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(sampler_linear, envProbesColorTexture, float4(normalize(R), float(fallbackTextureIndex)), lod);

            indirectLight += diffuseColor * (reflections.rgb * reflections.a);
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

#ifdef WRITE_MOMENTS
    output.output_moments = moments;
#endif // WRITE_MOMENTS

    return output;
}

#endif // PIXEL_SHADER
