#version 450
#extension GL_GOOGLE_include_directive : require

#include "ssr_header.inc"
#include "../include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 out_color;

HYP_DESCRIPTOR_SRV(RenderSSR, UVImage) uniform texture2D ssr_uv_image;

HYP_DESCRIPTOR_CBUFF(RenderSSR, UniformBuffer) uniform UniformBuffer
{
    uvec4 dimension;
    float ray_step;
    float num_iterations;
    float max_ray_distance;
    float distance_bias;
    float offset;
    float eye_fade_start;
    float eye_fade_end;
    float screen_edge_fade_start;
    float screen_edge_fade_end;

    float _pad0;
    float _pad1;
    float _pad2;
}
ssr_params;

HYP_DESCRIPTOR_SRV(RenderSSR, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(RenderSSR, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(RenderSSR, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
HYP_DESCRIPTOR_SRV(RenderSSR, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(RenderSSR, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;

HYP_DESCRIPTOR_SAMPLER(RenderSSR, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(RenderSSR, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SSBO(RenderSSR, BlueNoiseBuffer) readonly buffer BlueNoiseBuffer
{
    ivec4 sobol_256spp_256d[256 * 256 / 4];
    ivec4 scrambling_tile[128 * 128 * 8 / 4];
    ivec4 ranking_tile[128 * 128 * 8 / 4];
};

HYP_DESCRIPTOR_CBUFF(RenderSSR, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_CBUFF_DYNAMIC(RenderSSR, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

#include "../include/noise.inc"
#include "../include/shared.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/gbuffer.inc"
#include "../include/env_probe.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

float IsoscelesTriangleOpposite(float adjacent_length, float cone_theta)
{
    return 2.0 * tan(cone_theta) * adjacent_length;
}

float IsoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0 * h * h;

    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}

float IsoscelesTriangleNextAdjacent(float adjacent_length, float incircle_radius)
{
    // subtract the diameter of the incircle to get the adjacent side of the next level on the cone
    return adjacent_length - (incircle_radius * 2.0);
}

void main(void)
{
    const uvec2 coord = uvec2(gl_FragCoord.xy);
    const vec2 texcoord = v_texcoord;

    const vec2 ssr_image_dimensions = vec2(ssr_params.dimension.xy);

    vec4 uv_sample = Texture2D(sampler_nearest, ssr_uv_image, texcoord);
    const vec2 uv = uv_sample.xy;
    const float alpha = uv_sample.z;

    vec4 reflection_sample = vec4(0.0);
    float roughness = 0.0;

    float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;


    if (depth > 0.99999)
    {
        out_color = vec4(0.0);
        return;
    }

    vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth).xyz;
    const vec4 normalSample = Texture2D(sampler_nearest, gbuffer_normals_texture, texcoord);
    vec3 N =  GBufferUnpackNormal(normalSample);
    vec3 V = normalize(camera.position.xyz - P);

    if (alpha > HYP_FMATH_EPSILON)
    {
        uvec2 materialData = texture(usampler2D(gbuffer_material_texture, HYP_SAMPLER_NEAREST), texcoord).rg;

        GBufferMaterialParams materialParams;
        GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

        roughness = materialParams.roughness;
        roughness = clamp(roughness, 0.001, 0.999);

        const float perceptual_roughness = sqrt(roughness);

        const float gloss = 1.0 - roughness;
        const float cone_angle = RoughnessToConeAngle(perceptual_roughness) * 0.5;

        const float trace_size = float(max(ssr_params.dimension.x, ssr_params.dimension.y));
        const float max_mip_level = 9.0;

        vec2 sample_texcoord = texcoord; //+ (offset * filter_size);
        ivec2 sample_coord = ivec2(sample_texcoord * vec2(ssr_params.dimension.xy - 1) + 0.5);
        
        const vec4 hit_data = Texture2D(sampler_nearest, ssr_uv_image, sample_texcoord);
        const vec2 hit_uv = hit_data.xy;
        const float hit_mask = hit_data.z;
        const vec2 delta_p = (hit_uv - texcoord);

        float adjacent_length = length(delta_p);
        vec2 adjacent_unit = normalize(delta_p);

        float remaining_alpha = 1.0;
        float gloss_multiplier = gloss;

        vec4 accum_color = vec4(0.0);

        vec2 velocity = Texture2D(sampler_linear, gbuffer_velocity_texture, texcoord).xy;

#ifdef CONE_TRACING
        for (int i = 0; i < 14; i++)
        {
            const float opposite_length = IsoscelesTriangleOpposite(adjacent_length, cone_angle);
            const float incircle_size = IsoscelesTriangleInRadius(opposite_length, adjacent_length);
            const vec2 sample_position = texcoord + adjacent_unit * (adjacent_length - incircle_size);

            const float mip_level = clamp(log2(/*current_radius*/ incircle_size * max(ssr_image_dimensions.x, ssr_image_dimensions.y)), 0.0, max_mip_level);

            vec4 current_reflection_sample = Texture2DLod(sampler_linear, gbuffer_mip_chain, clamp(hit_uv - velocity, vec2(0.0), vec2(1.0)), mip_level);
#else
        const float current_radius = length((hit_uv - texcoord) * vec2(ssr_params.dimension.xy)) * tan(cone_angle);
        const float mip_level = clamp(log2(current_radius), 0.0, max_mip_level);

        vec4 current_reflection_sample = Texture2DLod(sampler_linear, gbuffer_mip_chain, clamp(hit_uv - velocity, vec2(0.0), vec2(1.0)), mip_level);
#endif

#ifdef CONE_TRACING
            current_reflection_sample.rgb *= vec3(gloss_multiplier);
            current_reflection_sample.a = gloss_multiplier;

            remaining_alpha -= current_reflection_sample.a;

            if (remaining_alpha < 0.0)
            {
                current_reflection_sample.rgb *= (1.0 - abs(remaining_alpha));
            }

            accum_color += current_reflection_sample;

            if (accum_color.a >= 1.0)
            {
                break;
            }

            adjacent_length = IsoscelesTriangleNextAdjacent(adjacent_length, incircle_size);
            gloss_multiplier *= gloss;
        }
#else
        accum_color = current_reflection_sample;
#endif

        reflection_sample = accum_color;
        reflection_sample.a = min(accum_color.a, 1.0);

        reflection_sample.a *= alpha;
    }

    out_color = reflection_sample;
}
