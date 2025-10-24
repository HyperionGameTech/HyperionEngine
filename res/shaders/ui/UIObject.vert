#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_screen_space_position;
layout(location = 2) out vec2 v_texcoord0;
layout(location = 3) out vec4 v_color;
layout(location = 4) out flat uint v_object_index;
layout(location = 5) out flat uvec4 v_properties;

#define INSTANCING

// clang-format off

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;

#include "../include/scene.inc"

#include "../include/object.inc"

#include "../include/UIObject.glsl"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO(Global, ObjectsBuffer) readonly buffer ObjectsBuffer
{
    Object objects[];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Instancing, EntityInstanceBatchesBuffer) readonly buffer EntityInstanceBatchesBuffer
{
    UIEntityInstanceBatch entity_instance_batch;
};

    // clang-format on

#undef OBJECT_INDEX
#define OBJECT_INDEX (entity_instance_batch.batch.indices[gl_InstanceIndex >> 2][gl_InstanceIndex & 3])

void main()
{
    vec2 clamped_offset = entity_instance_batch.offsets[gl_InstanceIndex].xy;
    vec2 size = entity_instance_batch.sizes[gl_InstanceIndex].xy;
    vec2 clamped_size = entity_instance_batch.sizes[gl_InstanceIndex].zw;

    vec4 position = entity_instance_batch.batch.transforms[gl_InstanceIndex] * vec4(a_position, 1.0);
    vec4 ndc_position = camera.projection * camera.view * position;

    // // scale texcoord based on the size diff - need to do this because the quad mesh is always 1x1
    vec4 instance_texcoords = entity_instance_batch.texcoords[gl_InstanceIndex];

    vec2 instance_texcoord_size = instance_texcoords.zw - instance_texcoords.xy;
    vec2 clamped_instance_texcoord_size = instance_texcoord_size * (clamped_size / size);

    v_texcoord0 = instance_texcoords.xy - (clamped_offset / clamped_size * clamped_instance_texcoord_size) + (a_texcoord0 * clamped_instance_texcoord_size);

    v_object_index = OBJECT_INDEX;
    v_properties = entity_instance_batch.properties[gl_InstanceIndex];

    v_position = position.xyz;
    v_screen_space_position = vec3(ndc_position.xy * 0.5 + 0.5, ndc_position.z);

    v_color = vec4(1.0);

    gl_Position = ndc_position;
}