#include "../include/Defines.hlsli"

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
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;
    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};


DECLARE_SRV(HBAO, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;
DECLARE_SRV(HBAO, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(HBAO, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(HBAO, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;

DECLARE_SRV(HBAO, GBufferMipChain) Texture2D GBufferMipChain;
DECLARE_SRV(HBAO, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SAMPLER(HBAO, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(HBAO, SamplerNearest) SamplerState sampler_nearest;

DECLARE_SRV(HBAO, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#include "../include/Shared.hlsli"
#include "../include/Scene.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/Packing.hlsli"
#include "../include/BlueNoise.hlsli"

DECLARE_SRV_DYNAMIC(HBAO, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(HBAO, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_BUFFER_DYNAMIC(HBAO, CBuffer) cbuffer CBuffer
{
    uint2 dimension;
    float radius;
    float power;
};

#define HYP_HBAO_NUM_CIRCLES 3
#define HYP_HBAO_NUM_SLICES 3

#define HYP_HBAO_NUM_TEMPORAL_SAMPLES 32u

#define HYP_HBAO_NOISE_DIMENSION_DIRECTION 0
#define HYP_HBAO_NOISE_DIMENSION_STEP 1

#define ANGLE_BIAS 0.1

float GetDepth(float2 uv)
{
    return SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferDepthTexture, uv, 0).r;
}

float3 GetPosition(float2 uv, float depth)
{
    return ReconstructViewSpacePositionFromDepth(camera.invProjMat, uv, depth).xyz;
}

float3 GetNormal(float2 uv)
{
    float3 normal = GBufferUnpackNormal(SAMPLE_TEXTURE_2D(sampler_nearest, GBufferNormalsTexture, uv));
    float3 view_normal = mul(camera.view, float4(normal, 0.0)).xyz;
    return normalize(view_normal);
}

float IntegrateUniformWeight(float2 h)
{
    float2 arc = float2(1.0, 1.0) - cos(h);
    return arc.x + arc.y;
}

float IntegrateArcCosWeight(float2 h, float n)
{
    float2 arc = -cos(2.0 * h - n) + cos(n) + 2.0 * h * sin(n);
    return 0.25 * (arc.x + arc.y);
}

float2 RotateDirection(float2 uv, float2 cos_sin)
{
    return float2(
        uv.x * cos_sin.x - uv.y * cos_sin.y,
        uv.x * cos_sin.y + uv.y * cos_sin.x);
}

float Falloff(float dist_sqr)
{
    return dist_sqr * (-1.0 / HYP_FMATH_SQR(radius)) + 1.0;
}

float2 CalculateImpact(float2 theta_0, float2 theta_1, float2 nx, float ny)
{
    float2 sin_theta_0 = sin(theta_0);
    float2 sin_theta_1 = sin(theta_1);
    float2 cos_theta_0 = cos(theta_0);
    float2 cos_theta_1 = cos(theta_1);

    const float2 dx = (theta_0 - sin_theta_0 * cos_theta_0) - (theta_1 - sin_theta_1 * cos_theta_1);
    const float2 dy = cos_theta_1 * cos_theta_1 - cos_theta_0 * cos_theta_0;

    return max(nx * dx + ny * dy, float2(0.0, 0.0));
}

void TraceAO_New(float2 uv, out float occlusion)
{
    const float fov_rad = HYP_FMATH_DEG2RAD(camera.fov);
    const float tan_half_fov = tan(fov_rad * 0.5);
    const float inv_tan_half_fov = 1.0 / tan_half_fov;
    
    uint2 pixel_coord = clamp(uint2(uv * float2(dimension)), 0, dimension - 1);

    const float projected_scale = float(dimension.y) / (tan_half_fov * 2.0);

    const int temporal_sample_index = int(world_shader_data.frame_counter % HYP_HBAO_NUM_TEMPORAL_SAMPLES);
    

    float2 rnd = float2(
        SampleBlueNoise(pixel_coord.x, pixel_coord.y, int(world_shader_data.frame_counter % HYP_HBAO_NUM_TEMPORAL_SAMPLES) * 2, HYP_HBAO_NUM_TEMPORAL_SAMPLES * 2),
        SampleBlueNoise(pixel_coord.x, pixel_coord.y, int(world_shader_data.frame_counter % HYP_HBAO_NUM_TEMPORAL_SAMPLES) * 2 + 1, HYP_HBAO_NUM_TEMPORAL_SAMPLES * 2)
    );
    
    const float noise_direction = rnd.x;
    const float ray_step = rnd.y;

    occlusion = 0.0;

    const float depth = GetDepth(uv);

    if (depth >= 1.0)
    {
        occlusion = 1.0;
        return;
    }

    const float3 P = GetPosition(uv, depth);
    const float3 N = GetNormal(uv);
    const float3 V = normalize(P);

    const float camera_distance = P.z;
    const float2 texel_size = float2(1.0, 1.0) / float2(dimension);
    const float step_radius = max((projected_scale * radius) / max(camera_distance, HYP_FMATH_EPSILON), float(HYP_HBAO_NUM_SLICES)) / float(HYP_HBAO_NUM_SLICES + 1);

    for (int i = 0; i < HYP_HBAO_NUM_CIRCLES; i++)
    {
        float angle = (float(i) + noise_direction) / float(HYP_HBAO_NUM_CIRCLES) * 2.0 * HYP_FMATH_PI;

        // direction we march along in UV space...
        float2 ss_ray = float2(sin(angle), cos(angle));
        float2 vs_ray = float2(ss_ray.x, -ss_ray.y);

        float3 ray = normalize(float3(vs_ray * V.z, -dot(V.xy, vs_ray)));
        const float nx = dot(ray, N);
        const float ny = max(-dot(N, V), 0.0);

        const float proj_len = max(length(float2(nx, ny)), HYP_FMATH_EPSILON);
        float2 cos_max_theta = float2(-nx, nx) / proj_len;
        float2 max_theta = acos(clamp(cos_max_theta, -1.0, 1.0));

        max_theta = max(float2(0.0, 0.0), max_theta - ANGLE_BIAS);
        cos_max_theta = cos(max_theta);

        float2 slice_ao = float2(0.0, 0.0);

        for (int j = 0; j < HYP_HBAO_NUM_SLICES; j++)
        {
            float2 uv_offset = (ss_ray * texel_size) * max(step_radius * (float(j) + ray_step), float(j + 1));

            float4 new_uv = uv.xyxy + float4(uv_offset, -uv_offset);

            const float4 new_uv_ndc = new_uv * 2.0 - 1.0;
            const float2 max_dimension = min(float2(1.0, 1.0), max(abs(new_uv_ndc.xz), abs(new_uv_ndc.yw)));
            const float2 fade = 1.0 - saturate(max(float2(0.0, 0.0), max_dimension - 0.9) / 0.1);

            bool valid_xy = all(new_uv.xy < float2(1.0, 1.0)) && all(new_uv.xy >= float2(0.0, 0.0));
            bool valid_zw = all(new_uv.zw < float2(1.0, 1.0)) && all(new_uv.zw >= float2(0.0, 0.0));

            if (valid_xy || valid_zw)
            {
                new_uv = saturate(new_uv);

                float2 in_bounds = float2(valid_xy ? 1.0 : 0.0, valid_zw ? 1.0 : 0.0);

                float depth_0 = GetDepth(new_uv.xy);
                float depth_1 = GetDepth(new_uv.zw);

                float3 ds = GetPosition(new_uv.xy, depth_0) - P;
                float3 dt = GetPosition(new_uv.zw, depth_1) - P;

                const float2 len = float2(length(ds), length(dt));
                const float2 safe_len = max(len, float2(HYP_FMATH_EPSILON, HYP_FMATH_EPSILON));
                const float2 dist = len / radius;
                const float2 DdotD = len * len;

                ds /= safe_len.x;
                dt /= safe_len.y;

                const float2 NdotD = float2(dot(ds, N), dot(dt, N));

                const float2 DdotV = float2(-dot(ds, V), -dot(dt, V));

                const float2 condition = float2(DdotV > cos_max_theta) * float2(dist < float2(1.0, 1.0));
                float2 falloffs = saturate(float2(Falloff(DdotD.x), Falloff(DdotD.y)));

                const float2 theta = acos(clamp(DdotV, -1.0, 1.0));

                // integrate the arc that this sample just swept away, from the new horizon up to the previous one
                const float2 impact = CalculateImpact(max_theta, theta, float2(nx, -nx), ny);
                const float2 total_impact = condition * falloffs * impact * in_bounds * fade;

                slice_ao += total_impact;

                cos_max_theta = lerp(cos_max_theta, DdotV, condition * in_bounds);
                max_theta = lerp(max_theta, theta, condition * in_bounds);
            }
        }

        occlusion += slice_ao.x + slice_ao.y;
    }

    occlusion = saturate(occlusion / float(2 * HYP_HBAO_NUM_CIRCLES));

    occlusion = saturate(occlusion * (1.0 / (1.0 - ANGLE_BIAS)));

    occlusion = pow(saturate(1.0 - occlusion), power);
}

float PSMain(PSInput input) : SV_Target0
{
    if (dimension.x == 0 || dimension.y == 0)
    {
        return 1.0;
    }
    
    float2 texcoord = input.texcoord;

    float occlusion;
    TraceAO_New(texcoord, occlusion);

    return occlusion;
}

#endif // PIXEL_SHADER
