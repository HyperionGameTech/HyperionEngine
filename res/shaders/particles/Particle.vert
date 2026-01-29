#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_texcoord0;
layout(location = 3) out vec4 v_color;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/Entity.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Particle.glsl"

#ifndef MAX_PARTICLES
#define MAX_PARTICLES 1024
#endif

DECLARE_UAV(ParticleDescriptorSet, ParticlesBuffer, standard = std430) buffer ParticlesBuffer
{
    ParticleShaderData instances[MAX_PARTICLES];
};

DECLARE_BUFFER(ParticleDescriptorSet, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_BUFFER_DYNAMIC(ParticleDescriptorSet, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

void main()
{
    const int instance_id = gl_InstanceIndex;

    vec4 position;
    mat4 normal_matrix;

    position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_normal = a_normal;
    // v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);

    ParticleShaderData instance = instances[instance_id];

    const vec4 particle_position_world = instance.position;
    position.xyz *= particle_position_world.w;

    const vec3 lookat_dir = normalize(camera.position.xyz - particle_position_world.xyz);
    const vec3 lookat_z = lookat_dir;
    const vec3 lookat_x = normalize(cross(vec3(0.0, 1.0, 0.0), lookat_dir));
    const vec3 lookat_y = normalize(cross(lookat_dir, lookat_x));

    const mat4 lookat_matrix = {
        vec4(lookat_x, 0.0),
        vec4(lookat_y, 0.0),
        vec4(lookat_z, 0.0),
        vec4(vec3(0.0), 1.0)
    };

    position = lookat_matrix * position;
    position.xyz += particle_position_world.xyz;

    v_color = instance.color;

    gl_Position = camera.viewProjMat * position;
}
