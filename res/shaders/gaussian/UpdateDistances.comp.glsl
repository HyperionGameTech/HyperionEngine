#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

#include "../include/shared.inc"
#include "../include/defines.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/Entity.glsl"
#include "../include/packing.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Gaussian.inc.glsl"

HYP_DESCRIPTOR_CBUFF(UpdateDistancesDescriptorSet, GaussianSplattingSceneShaderData, standard = std430) uniform GaussianSplattingSceneShaderData
{
    mat4 model_matrix;
};

HYP_DESCRIPTOR_SSBO(UpdateDistancesDescriptorSet, SplatIndicesBuffer, standard = std430) buffer SplatIndicesBuffer
{
    GaussianSplatIndex splat_indices[];
};

HYP_DESCRIPTOR_SSBO(UpdateDistancesDescriptorSet, SplatInstancesBuffer, standard = std430) buffer SplatInstancesBuffer
{
    GaussianSplatShaderData instances[];
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

layout(push_constant) uniform PushConstant
{
    uint num_points;
}
push_constants;

void main()
{
    const uint index = gl_GlobalInvocationID.x;

    if (index >= push_constants.num_points)
    {
        return;
    }

    GaussianSplatShaderData instance = instances[index];

    vec3 camera_position = camera.position.xyz;
    vec3 splat_position = instance.position.xyz;

    float dist = (camera.view * model_matrix * vec4(splat_position, 1.0)).z; // 0.0;//distance(camera.position.xyz, splat_position);//

    GaussianSplatIndex gaussian_splat_index;
    gaussian_splat_index.index = index;
    gaussian_splat_index.distance = dist >= 0.0 ? dist : 999999.0;

    splat_indices[index] = gaussian_splat_index;
}