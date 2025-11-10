#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec4 v_color;
layout(location = 2) out vec4 v_conic_radius;
layout(location = 3) out vec2 v_center_screen_position;
layout(location = 4) out vec2 v_quad_position;
layout(location = 5) out vec2 v_uv;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec3 a_tangent;
layout(location = 5) in vec3 a_bitangent;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/Entity.glsl"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Gaussian.inc.glsl"

HYP_DESCRIPTOR_CBUFF(GaussianSplattingDescriptorSet, GaussianSplattingSceneShaderData, standard = std430) uniform GaussianSplattingSceneShaderData
{
    mat4 model_matrix;
};

HYP_DESCRIPTOR_SSBO(GaussianSplattingDescriptorSet, SplatIndicesBuffer, standard = std430) buffer SplatIndicesBuffer
{
    GaussianSplatIndex splat_indices[];
};

HYP_DESCRIPTOR_SSBO(GaussianSplattingDescriptorSet, SplatInstancesBuffer, standard = std430) buffer SplatInstancesBuffer
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

void CalcCovariance3D(mat3 rot, out vec3 sigma0, out vec3 sigma1)
{
    mat3 sig = transpose(rot) * (rot);
    sigma0 = vec3(sig[0][0], sig[0][1], sig[0][2]);
    sigma1 = vec3(sig[1][1], sig[1][2], sig[2][2]);
}

vec3 CalcCovariance2D(vec3 worldPos, vec3 cov3d0, vec3 cov3d1)
{
    mat4 view = camera.view;
    vec4 viewPos = view * vec4(worldPos, 1.0);

    float aspect = camera.projection[0][0] / camera.projection[1][1];
    float tanFovX = 1.0 / camera.projection[0][0];
    float tanFovY = aspect / camera.projection[1][1];
    float limX = 1.3 * tanFovX;
    float limY = 1.3 * tanFovY;
    viewPos.x = clamp(viewPos.x / viewPos.z, -limX, limX) * viewPos.z;
    viewPos.y = clamp(viewPos.y / viewPos.z, -limY, limY) * viewPos.z;

    float focal = camera.dimensions.x * camera.projection[0][0] / 2.0;

    mat4 J = mat4(
        focal / viewPos.z, 0.0, -(focal * viewPos.x) / (viewPos.z * viewPos.z), 0.0,
        0.0, focal / viewPos.z, -(focal * viewPos.y) / (viewPos.z * viewPos.z), 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0);

    // view[0][3] = 0.0;
    // view[1][3] = 0.0;
    // view[2][3] = 0.0;

    mat4 W = transpose(view);
    mat4 T = W * J;
    mat4 V = mat4(
        cov3d0.x, cov3d0.y, cov3d0.z, 0.0,
        cov3d0.y, cov3d1.x, cov3d1.y, 0.0,
        cov3d0.z, cov3d1.y, cov3d1.z, 0.0,
        0.0, 0.0, 0.0, 0.0);
    mat4 cov = transpose(T) * transpose(V) * T;

    // Low pass filter to make each splat at least 1px size.
    cov[0][0] += 0.3;
    cov[1][1] += 0.3;
    return vec3(cov[0][0], cov[0][1], cov[1][1]);
}

mat3 CalcMatrixFromRotationScale(vec4 rot, vec3 scale, float scale_modifier)
{
    mat3 ms = mat3(
        exp(scale.x) * scale_modifier, 0.0, 0.0,
        0.0, exp(scale.y) * scale_modifier, 0.0,
        0.0, 0.0, exp(scale.z) * scale_modifier);
    float w = rot.w;
    float r = rot.w;
    float x = rot.x;
    float y = rot.y;
    float z = rot.z;

    mat3 mr = mat3(
        /*1.0 - 2.0 * (y * y + z * z),    2.0 * (x * y + w * z),          2.0 * (x * z - w * y),
        2.0 * (x * y - w * z),          1.0 - 2.0 * (x * x + z * z),    2.0 * (y * z + w * x),
        2.0 * (x * z + w * y),          2.0 * (y * z - w * x),          1.0 - 2.0 * (x * x + y * y)*/

        1. - 2. * (y * y + z * z), 2. * (x * y - r * z), 2. * (x * z + r * y),
        2. * (x * y + r * z), 1. - 2. * (x * x + z * z), 2. * (y * z - r * x),
        2. * (x * z - r * y), 2. * (y * z + r * x), 1. - 2. * (x * x + y * y));

    return (ms * mr);
}

float rcp(float x)
{
    return 1.0 / x;
}

void main()
{
    const int instance_id = gl_InstanceIndex;
    const GaussianSplatIndex gaussian_splat_index = splat_indices[instance_id];

    const vec2 quad_position = a_position.xy;

    GaussianSplatShaderData instance = instances[gaussian_splat_index.index];

    vec3 world_position = (model_matrix * vec4(instance.position.xyz, 1.0)).xyz;

    vec4 rotation = instance.rotation;
    vec3 scale = instance.scale.xyz;

    mat3 rotation_scale_matrix = CalcMatrixFromRotationScale(rotation, scale, 1.0) * mat3(model_matrix);

    vec3 covariance_3d_0;
    vec3 covariance_3d_1;

    CalcCovariance3D(rotation_scale_matrix, covariance_3d_0, covariance_3d_1);

    vec3 covariance_2d = CalcCovariance2D(
        world_position,
        covariance_3d_0,
        covariance_3d_1);

    float det = covariance_2d.x * covariance_2d.z - covariance_2d.y * covariance_2d.y;

    float mid = 0.5 * (covariance_2d.x + covariance_2d.z);
    float lambda1 = mid + sqrt(max(0.1, mid * mid - det));
    float lambda2 = mid - sqrt(max(0.1, mid * mid - det));
    float radius = ceil(3.0 * sqrt(max(lambda1, lambda2)));

    vec3 conic = vec3(covariance_2d.z, -covariance_2d.y, covariance_2d.x) * rcp(det);
    const vec4 conic_radius = vec4(conic, radius);

    vec4 center_ndc_position = camera.projection * camera.view * vec4(world_position, 1.0);
    center_ndc_position /= center_ndc_position.w;

    vec2 center_screen_position = (center_ndc_position.xy * 0.5 + 0.5) * camera.dimensions.xy;

    vec2 delta_screen_position = quad_position * radius * 2.0 / camera.dimensions.xy;

    gl_Position = center_ndc_position + vec4(delta_screen_position, 0.0, 0.0);

    v_color = instance.color; // vec4(vec3(splat_distances[instance_id >> 2][instance_id & 3], 0.0, 0.0), 1.0); ////
    v_position = gl_Position.xyz;
    v_quad_position = quad_position;
    v_conic_radius = conic_radius;
    v_uv = vec2(quad_position * radius);
}
