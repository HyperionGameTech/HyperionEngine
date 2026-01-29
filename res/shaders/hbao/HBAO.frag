#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;
layout(location = 0) out vec4 color_output;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/noise.inc"
#include "../include/packing.inc"

DECLARE_SRV(HBAO, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
DECLARE_SRV(HBAO, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
DECLARE_SRV(HBAO, GBufferMaterialTexture) uniform texture2D gbuffer_material_texture;
DECLARE_SRV(HBAO, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;

DECLARE_SRV(HBAO, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
DECLARE_SRV(HBAO, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
DECLARE_SAMPLER(HBAO, SamplerLinear) uniform sampler sampler_linear;
DECLARE_SAMPLER(HBAO, SamplerNearest) uniform sampler sampler_nearest;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/gbuffer.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(HBAO, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(HBAO, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_BUFFER(HBAO, UniformBuffer) uniform UniformBuffer
{
    uvec2 dimension;
    float radius;
    float power;
};

#include "../include/Temporal.inc"

#define HYP_HBAO_NUM_CIRCLES 4
#define HYP_HBAO_NUM_SLICES 2

float fov_rad = HYP_FMATH_DEG2RAD(camera.fov);
float tan_half_fov = tan(fov_rad * 0.5);
float inv_tan_half_fov = 1.0 / tan_half_fov;
vec2 focal_len = vec2(inv_tan_half_fov * (float(camera.dimensions.y) / float(camera.dimensions.x)), inv_tan_half_fov);
vec2 inv_focal_len = vec2(1.0) / focal_len;
vec4 uv_to_view = vec4(2.0 * inv_focal_len.x, 2.0 * inv_focal_len.y, -1.0 * inv_focal_len.x, -1.0 * inv_focal_len.y);

float GetOffsets(vec2 uv)
{
    ivec2 position = ivec2(uv * vec2(dimension));
    return 0.25 * float((position.y - position.x) & 3);
}

mat4 inv_view_proj = inverse(camera.viewProjMat);
mat4 inv_view = inverse(camera.view);
mat4 inv_proj = inverse(camera.projection);

float GetDepth(vec2 uv)
{
    return SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, uv).r;
}

vec3 GetPosition(vec2 uv, float depth)
{
    return ReconstructViewSpacePositionFromDepth(inv_proj, uv, depth).xyz;
}

vec3 GetNormal(vec2 uv)
{
    vec3 normal = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, uv));
    vec3 view_normal = (camera.view * vec4(normal, 0.0)).xyz;

    return view_normal;
}

float IntegrateUniformWeight(vec2 h)
{
    vec2 arc = vec2(1.0) - cos(h);

    return arc.x + arc.y;
}

float IntegrateArcCosWeight(vec2 h, float n)
{
    vec2 arc = -cos(2.0 * h - n) + cos(n) + 2.0 * h * sin(n);

    return 0.25 * (arc.x + arc.y);
}

vec2 RotateDirection(vec2 uv, vec2 cos_sin)
{
    return vec2(
        uv.x * cos_sin.x - uv.y * cos_sin.y,
        uv.x * cos_sin.y + uv.y * cos_sin.x);
}

#define ANGLE_BIAS 0.0

float Falloff(float dist_sqr)
{
    return dist_sqr * (-1.0 / HYP_FMATH_SQR(radius)) + 1.0;
}

vec2 CalculateImpact(vec2 theta_0, vec2 theta_1, float nx, float ny)
{
    vec2 sin_theta_0 = sin(theta_0);
    vec2 sin_theta_1 = sin(theta_1);
    vec2 cos_theta_0 = cos(theta_0);
    vec2 cos_theta_1 = cos(theta_1);

    const vec2 dx = (theta_0 - sin_theta_0 * cos_theta_0) - (theta_1 - sin_theta_1 * cos_theta_1);
    const vec2 dy = cos_theta_1 * cos_theta_1 - cos_theta_0 * cos_theta_0;

    return max(nx * dx + ny * dy, vec2(0.0));
}

#ifdef HBIL_ENABLED
void TraceAO_New(vec2 uv, out float occlusion, out vec4 light_color)
#else
void TraceAO_New(vec2 uv, out float occlusion)
#endif
{
    uvec2 pixel_coord = uvec2(clamp(ivec2(uv * vec2(dimension) - 0.5), ivec2(0), ivec2(dimension) - 1));
    uint seed = InitRandomSeed(InitRandomSeed(pixel_coord.x, pixel_coord.y), world_shader_data.frame_counter % 256);

    const float projected_scale = float(dimension.y) / (tan_half_fov * 2.0);

    const float temporal_offset = GetSpatialOffset(world_shader_data.frame_counter);
    const float temporal_rotation = GetTemporalRotation(world_shader_data.frame_counter);

    const float noise_offset = GetOffsets(uv);
    const float noise_direction = InterleavedGradientNoise(vec2(pixel_coord));
    const float ray_step = fract(noise_offset + temporal_offset);

    const float depth = GetDepth(uv);
    const vec3 P = GetPosition(uv, depth);
    const vec3 N = GetNormal(uv);
    const vec3 V = normalize(P);

    const float camera_distance = P.z;
    const vec2 texel_size = vec2(1.0) / vec2(dimension);
    const float step_radius = max((projected_scale * radius) / max(camera_distance, HYP_FMATH_EPSILON), float(HYP_HBAO_NUM_SLICES)) / float(HYP_HBAO_NUM_SLICES + 1);

    occlusion = 0.0;

#ifdef HBIL_ENABLED
    light_color = vec4(0.0);
#endif

    for (int i = 0; i < HYP_HBAO_NUM_CIRCLES; i++)
    {
        // float angle = (float(i) + noise_direction) / float(HYP_HBAO_NUM_CIRCLES) * 2.0 * HYP_FMATH_PI;//(float(i) + noise_direction + ((temporal_rotation / 360.0))) * (HYP_FMATH_PI / float(HYP_HBAO_NUM_CIRCLES)) * 2.0;
        float angle = (float(i) + noise_direction + ((temporal_rotation / 360.0))) * (HYP_FMATH_PI / float(HYP_HBAO_NUM_CIRCLES)) * 2.0;

        vec2 ss_ray = vec2(sin(angle), cos(angle));
        vec3 ray = normalize(vec3(ss_ray.xy * V.z, -dot(V.xy, ss_ray.xy)));
        const float nx = dot(ray, N);
        const float ny = -dot(N, V);
        const float ctg_th0 = -nx / ny;

        vec2 cos_max_theta = vec2(ctg_th0 * inversesqrt(ctg_th0 * ctg_th0 + 1.0));
        vec2 max_theta = vec2(acos(cos_max_theta));

        const float start_angle_delta = 0.0;
        max_theta = vec2(max(vec2(0.0), max_theta - start_angle_delta));
        cos_max_theta = cos(max_theta);

        // const vec4 integral_factors = vec4(vec2(dot(N.xy, ss_ray), N.z), vec2(dot(N.xy, -ss_ray), N.z));

        vec2 slice_ao = vec2(0.0);

#ifdef HBIL_ENABLED
        vec4 slice_light[2] = { vec4(0.0), vec4(0.0) };
#endif

        for (int j = 0; j < HYP_HBAO_NUM_SLICES; j++)
        {
            vec2 uv_offset = (ss_ray * texel_size) * max(step_radius * (float(j) + ray_step), float(j + 1));

            vec4 new_uv = uv.xyxy + vec4(uv_offset, -uv_offset);

            // Fade hits that approach the screen edge
            const vec4 new_uv_ndc = new_uv * 2.0 - 1.0;
            const float max_dimension = min(1.0, max(abs(new_uv_ndc.x), abs(new_uv_ndc.y)));
            const float fade = 1.0 - saturate(max(0.0, max_dimension - 0.95) / (1.0 - 0.98));

            if (all(lessThan(new_uv.xy, vec2(1.0))) && all(greaterThanEqual(new_uv.xy, vec2(0.0))))
            {
                new_uv = saturate(new_uv);

                float depth_0 = GetDepth(new_uv.xy);
                float depth_1 = GetDepth(new_uv.zw);

                vec3 ds = GetPosition(new_uv.xy, depth_0) - P;
                vec3 dt = GetPosition(new_uv.zw, depth_1) - P;

                const vec2 len = vec2(length(ds), length(dt));
                const vec2 dist = len / radius;
                ds /= len.x;
                dt /= len.y;

                const vec2 DdotD = vec2(dot(ds, ds), dot(dt, dt));
                const vec2 NdotD = vec2(dot(ds, N), dot(dt, N));

                // const vec2 H = NdotD * inversesqrt(DdotD);

                const vec2 DdotV = vec2(-dot(ds, V), -dot(dt, V));

                const vec2 condition = vec2(greaterThan(DdotV, cos_max_theta)) * vec2(lessThan(dist, vec2(1.0)));
                vec2 falloffs = saturate(vec2(Falloff(DdotD.x), Falloff(DdotD.y)));

                const vec2 theta = acos(DdotV);

                const vec2 impact = CalculateImpact(min(max_theta, theta + vec2(HYP_FMATH_PI / 9.0)), theta, nx, ny);
                const vec2 total_impact = condition * falloffs * impact;

#ifdef HBIL_ENABLED
                vec4 new_color_0 = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_albedo_texture, new_uv.xy);
                vec4 new_color_1 = SAMPLE_TEXTURE_2D(sampler_linear, gbuffer_albedo_texture, new_uv.zw);

                slice_light[0] += vec4(new_color_0) * total_impact.x;
                slice_light[1] += vec4(new_color_1) * total_impact.y;
#endif

                slice_ao += vec2(
                                (1.0 - dist.x * dist.x) * (NdotD.x - slice_ao.x),
                                (1.0 - dist.y * dist.y) * (NdotD.y - slice_ao.y))
                    * condition * fade;

                cos_max_theta = mix(cos_max_theta, DdotV, condition);
                max_theta = mix(max_theta, theta, condition);
            }
        }

#ifdef HBIL_ENABLED
        light_color += slice_light[0] + slice_light[1];
#endif

        occlusion += slice_ao.x + slice_ao.y;
    }

    occlusion = 1.0 - saturate(pow(occlusion / float(HYP_HBAO_NUM_CIRCLES * HYP_HBAO_NUM_SLICES), 1.0 / power));
    occlusion *= 1.0 / (1.0 - ANGLE_BIAS);

#ifdef HBIL_ENABLED
    light_color = light_color / float(HYP_HBAO_NUM_CIRCLES * HYP_HBAO_NUM_SLICES);
    light_color *= 1.0 / (1.0 - ANGLE_BIAS);
#endif
}

void main()
{
    vec2 texcoord = v_texcoord;

    vec4 values;
    float occlusion;

#ifdef HBIL_ENABLED
    vec4 light_color;

    TraceAO_New(texcoord, occlusion, light_color);

    values = vec4(light_color.rgb, occlusion);
#else
    TraceAO_New(texcoord, occlusion);

    values = vec4(occlusion);
#endif

    color_output = values;
}