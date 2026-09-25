#include "include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/Gbuffer.hlsli"
#include "include/Scene.hlsli"
#include "include/Entity.hlsli"
#include "include/Packing.hlsli"
#include "include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
};

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation uint object_index : TEXCOORD1;
};

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
#endif // INSTANCING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

#ifdef INSTANCING
    // We don't use this for sky.
    // Dummy to allow this to compile when precompiling shaders AOT.
    MeshEntityInstanceBatch batch = (MeshEntityInstanceBatch) 0;
#endif // INSTANCING

    float4 position = mul(entity.model_matrix, float4(input.a_position, 1.0));

    float3x3 normal_matrix = (float3x3)entity.normal_matrix;

    output.position = input.a_position.xyz;
    output.normal = mul(normal_matrix, input.a_normal);
    output.texcoord0 = input.a_texcoord0;

    float4x4 view_matrix = camera.view;
    
    // strip the translation from the view matrix
    view_matrix[0][3] = 0.0;
    view_matrix[1][3] = 0.0;
    view_matrix[2][3] = 0.0;

#ifdef INSTANCING
    output.object_index = OBJECT_INDEX;
#else
    output.object_index = 0;
#endif

    output.position_cs = mul(camera.projection, mul(view_matrix, position));

    ///pins sky to the far-plane
    output.position_cs.z = output.position_cs.w;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation uint object_index : TEXCOORD1;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState texture_sampler;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) TextureCube textures[]; // aliasing texture2D as textureCube
#else // !HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Default, DiffuseMap) TextureCube DiffuseMap;
#endif // HYP_FEATURES_BINDLESS_TEXTURES

DECLARE_SRV(Default, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "include/Atmosphere.hlsli"

// a little bigger than the real sun (about 0.27 degrees) so it reads at game resolutions
static const float SunDiskInnerCosAngle = 0.99997563; // cos(0.4 degrees)
static const float SunDiskOuterCosAngle = 0.99996192; // cos(0.5 degrees)

// physically the disk would be ~1e5 times brighter than the sun's irradiance, which only blows out bloom
static const float SunDiskRadianceScale = 20.0;

float3 GetSunDisk(float3 rayDirection)
{
    const uint requiredFlags = WORLD_ENVIRONMENT_FLAG_HAS_SUN | WORLD_ENVIRONMENT_FLAG_SUN_DISK;

    if ((world_shader_data.environment_flags & requiredFlags) != requiredFlags)
    {
        return (float3)0.0;
    }

    const float3 directionToSun = normalize(world_shader_data.sun_direction_intensity.xyz);
    const float diskMask = smoothstep(SunDiskOuterCosAngle, SunDiskInnerCosAngle, dot(rayDirection, directionToSun));

    if (diskMask <= 0.0)
    {
        return (float3)0.0;
    }

    // an overcast sky has no visible sun, even where the clouds thin out
    const float overcastWeight = smoothstep(0.5, 1.0, world_shader_data.sky_light_params.w);

    return world_shader_data.sun_color.rgb
        * world_shader_data.sun_direction_intensity.w
        * GetSunTransmittance(0.0, directionToSun)
        * (diskMask * (1.0 - overcastWeight) * SunDiskRadianceScale);
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float3 rayDirection = normalize(input.position);

    const float3 skyRadiance = SAMPLE_MATERIAL_TEXTURE_CUBE(material, DiffuseMap, input.position).rgb;

    output.gbuffer_albedo = float4(skyRadiance + GetSunDisk(rayDirection), 1.0);
    output.gbuffer_normals = GBufferPackNormal(-rayDirection);
    output.gbuffer_material = OBJECT_MASK_UNLIT << 28u;
    output.gbuffer_velocity = (float2)0.0;

    return output;
}

#endif // PIXEL_SHADER
