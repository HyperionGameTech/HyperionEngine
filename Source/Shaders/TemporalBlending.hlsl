#include "include/defines.inc"
#include "include/packing.inc"
#include "include/shared.inc"

PERMUTE(OUTPUT, RGBA8, RGBA16F, RGBA32F);
PERMUTE(TEMPORAL_BLEND_TECHNIQUE, 0, 1, 2, 3, 4);

// #define PASSTHROUGH

#ifndef OUTPUT_FORMAT
    #if defined(OUTPUT_RGBA8)
        #define OUTPUT_FORMAT rgba8
        #define TEMPORAL_BLENDING_GAMMA_CORRECTION
    #elif defined(OUTPUT_RGBA16F)
        #define OUTPUT_FORMAT rgba16f
    #elif defined(OUTPUT_RGBA32F)
        #define OUTPUT_FORMAT rgba32f
    #else
        #define OUTPUT_FORMAT rgba8
        // #define TEMPORAL_BLENDING_REVERSE_TONEMAP
    #endif
#endif

#ifndef TEMPORAL_BLEND_TECHNIQUE
    #define TEMPORAL_BLEND_TECHNIQUE 0
#endif

#if defined(OUTPUT_RGBA8) || !defined(OUTPUT_RGBA16F) && !defined(OUTPUT_RGBA32F)
    #define OUTPUT_UAV_TYPE unorm float4
#else
    #define OUTPUT_UAV_TYPE float4
#endif

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/scene.inc"

DECLARE_SRV(TemporalBlending, InImage) Texture2D input_texture;
DECLARE_SRV(TemporalBlending, PrevImage) Texture2D prev_input_texture;
DECLARE_SRV(TemporalBlending, VelocityImage) Texture2D velocity_texture;
DECLARE_SAMPLER(TemporalBlending, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(TemporalBlending, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(TemporalBlending, OutImage) RWTexture2D<OUTPUT_UAV_TYPE> output_image;

DECLARE_SRV(TemporalBlending, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV_DYNAMIC(TemporalBlending, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(TemporalBlending, GBufferDepthTexture) Texture2D depth_texture;

DECLARE_BUFFER(TemporalBlending, TemporalBlendingUniforms) cbuffer TemporalBlendingUniforms
{
    uint2 output_dimensions;
    uint2 depth_texture_dimensions;
    uint blending_frame_counter;
};

#define TEMPORAL_BLENDING_USE_YCoCg
// #define ADJUST_COLOR_HDR

#include "include/Temporal.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 coord = dispatchThreadID.xy;

    if (any(coord >= output_dimensions))
    {
        return;
    }

    const float2 uv = (float2(coord) + 0.5) / float2(output_dimensions);
    const float2 texel_size = float2(1.0, 1.0) / float2(output_dimensions);

    float4 color;

    float2 velocity;
    float view_space_depth;

    InitTemporalParams(
        depth_texture,
        velocity_texture,
        depth_texture_dimensions,
        uv,
        camera.near,
        camera.far,
        velocity,
        view_space_depth);

#ifdef PASSTHROUGH
    color = SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv);
#elif TEMPORAL_BLEND_TECHNIQUE == 0
    color = SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv);//RGBToYCoCg(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv)));
    float4 previous_color = SAMPLE_TEXTURE_2D(sampler_linear, prev_input_texture, uv - velocity);//RGBToYCoCg(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, prev_input_texture, uv - velocity)));

    const float _SubpixelThreshold = 0.5;
    const float _GatherBase = 0.5;
    const float _GatherSubpixelMotion = 0.1666;

    const float2 texel_vel = velocity / max(float2(HYP_FMATH_EPSILON, HYP_FMATH_EPSILON), texel_size);
    const float texel_vel_mag = length(texel_vel) * view_space_depth;
    const float subpixel_motion = saturate(_SubpixelThreshold / max(HYP_FMATH_EPSILON, texel_vel_mag));
    const float min_max_support = _GatherBase + _GatherSubpixelMotion * subpixel_motion;

    float4 mean = color;
    float4 stddev = color * color;

    const float2 ss_offset01 = min_max_support * float2(-texel_size.x, texel_size.y);
    const float2 ss_offset11 = min_max_support * float2(texel_size.x, texel_size.y);

    float2 offsets[4];
    offsets[0] = -ss_offset11;
    offsets[1] = -ss_offset01;
    offsets[2] = ss_offset01;
    offsets[3] = ss_offset11;

    for (int i = 0; i < 4; i++)
    {
        float4 c = SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv + offsets[i]);  //RGBToYCoCg(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv + offsets[i])));

        mean += c;
        stddev += c * c;
    }

    mean /= 5.0;
    stddev = sqrt(max(stddev / 5.0 - (mean * mean), 0.0));

    // Use a gamma factor to widen the AABB slightly, reducing flickering
    const float variance_gamma = 1.0;
    float4 clipped = ClipToAABB(color, previous_color, mean, stddev * variance_gamma);

    //color = TemporalLuminanceResolveYCoCg(color, clipped);
    //color = YCoCgToRGB(color);
    //color = ADJUST_COLOR_GAMMA_OUT(color);

    color = any(isnan(color)) ? float4(0.0, 0.0, 1.0, 1.0) : color;

#elif TEMPORAL_BLEND_TECHNIQUE == 1
    color = TemporalResolve(input_texture, prev_input_texture, uv, velocity, texel_size, view_space_depth);

#elif TEMPORAL_BLEND_TECHNIQUE == 2
    color = TemporalBlendRounded(input_texture, prev_input_texture, uv, velocity, texel_size, view_space_depth);

#elif TEMPORAL_BLEND_TECHNIQUE == 3
    color = TemporalBlendVarying(input_texture, prev_input_texture, uv, velocity, texel_size, view_space_depth);

#elif TEMPORAL_BLEND_TECHNIQUE == 4
    color = SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv);
    float4 previous_color = SAMPLE_TEXTURE_2D(sampler_linear, prev_input_texture, uv);

    // Moving average over number of frames
    if (blending_frame_counter > 0u)
    {
        float alpha = 1.0 / float(blending_frame_counter + 1u);
        color = lerp(previous_color, color, alpha);
    }

    color.a = 1.0;
#endif

    output_image[coord] = color;
}
