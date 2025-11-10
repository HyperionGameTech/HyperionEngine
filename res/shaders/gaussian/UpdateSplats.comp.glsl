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

struct IndirectDrawCommand
{
    // VkDrawIndexedIndirectCommand
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

HYP_DESCRIPTOR_SSBO(UpdateSplatsDescriptorSet, IndirectDrawCommandsBuffer, standard = scalar) buffer IndirectDrawCommandsBuffer
{
    IndirectDrawCommand indirect_draw_command;
};

HYP_DESCRIPTOR_CBUFF(UpdateSplatsDescriptorSet, GaussianSplattingSceneShaderData, standard = std430) uniform GaussianSplattingSceneShaderData
{
    mat4 model_matrix;
};

HYP_DESCRIPTOR_SSBO(UpdateSplatsDescriptorSet, SplatIndicesBuffer, standard = std430) buffer SplatIndicesBuffer
{
    GaussianSplatIndex splat_indices[];
};

HYP_DESCRIPTOR_SSBO(UpdateSplatsDescriptorSet, SplatInstancesBuffer, standard = std430) buffer SplatInstancesBuffer
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
    const uint id = gl_GlobalInvocationID.x;
    const uint index = id;

    if (id >= push_constants.num_points)
    {
        return;
    }

    GaussianSplatIndex splat_index = splat_indices[index];

    if (splat_index.distance >= 1000.0 || splat_index.distance < 0.0)
    {
        return;
    }

    atomicAdd(indirect_draw_command.instance_count, 1u);
}