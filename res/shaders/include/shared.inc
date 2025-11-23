#ifndef HYP_SHARED_GLSL
#define HYP_SHARED_GLSL

#include "defines.inc"

// HLSL Compat
#define saturate(_x) Saturate(_x)
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4

#define HYP_FAST_LESS(x, y) (1 - step((y), (x)))

struct Ray
{
    vec3 origin;
    vec3 direction;
};

int imod(int a, int b)
{
    return (a % b + b) % b;
}

ivec2 imod(ivec2 a, ivec2 b)
{
    return (a % b + b) % b;
}

ivec3 imod(ivec3 a, ivec3 b)
{
    return (a % b + b) % b;
}

ivec4 imod(ivec4 a, ivec4 b)
{
    return (a % b + b) % b;
}

#define fmod(x, y) ((x) - (y)*floor((x) / (y)))

#define round(x) floor((x) + 0.5)

// #define rcp(x) (1.0 / (x))

float Luminance(vec3 color)
{
    return color.r * 0.2125 + color.g * 0.715 + color.b * 0.0721;
}

#define AngleBetweenVectors(a, b) (acos(dot((a), (b))))

mat4 CreateRotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s, 0.0,
        oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
        oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c, 0.0,
        0.0, 0.0, 0.0, 1.0);
}

vec3 RotateVector(vec3 v, vec3 axis, float angle)
{
    mat4 m = CreateRotationMatrix(axis, angle);
    return (m * vec4(v, 1.0)).xyz;
}

void ComputeOrthonormalBasis(in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    vec3 T;
    T = cross(normal, vec3(0.0, 1.0, 0.0));
    T = mix(cross(normal, vec3(1.0, 0.0, 0.0)), T, step(HYP_FMATH_EPSILON, dot(T, T)));

    T = normalize(T);

    tangent = T;
    bitangent = normalize(cross(normal, T));
}

#ifndef VERTEX_SHADER

vec4 Texture2D(sampler2D tex, vec2 texcoord)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(tex, vec2(texcoord.s, 1.0 - texcoord.t));
#else
    return texture(tex, texcoord);
#endif
}

vec4 Texture2DLod(sampler2D tex, vec2 texcoord, float lod)
{
#if HYP_GBUFFER_FLIP_Y
    return textureLod(tex, vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return textureLod(tex, texcoord, lod);
#endif
}

vec4 Texture2D(sampler samp, texture2D tex, vec2 texcoord)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(sampler2D(tex, samp), vec2(texcoord.s, 1.0 - texcoord.t));
#else
    return texture(sampler2D(tex, samp), texcoord);
#endif
}

vec4 Texture2DLod(sampler samp, texture2D tex, vec2 texcoord, float lod)
{
#if HYP_GBUFFER_FLIP_Y
    return textureLod(sampler2D(tex, samp), vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return textureLod(sampler2D(tex, samp), texcoord, lod);
#endif
}

vec4 Texture2DTexel(sampler2D tex, ivec2 coord)
{
    return texelFetch(tex, coord, 0);
}

vec4 Texture2DTexel(sampler samp, texture2D tex, ivec2 coord)
{
    return texelFetch(sampler2D(tex, samp), coord, 0);
}

vec4 Texture2DTexelLod(sampler samp, texture2D tex, ivec2 coord, int lod)
{
    return texelFetch(sampler2D(tex, samp), coord, lod);
}

vec4 Texture2DGather(sampler2D tex, vec2 coord)
{
    return textureGather(tex, coord);
}

vec4 Texture3D(sampler samp, texture3D tex, vec3 texcoord)
{
    return texture(sampler3D(tex, samp), texcoord);
}

vec4 Texture3DLod(sampler samp, texture3D tex, vec3 texcoord, float lod)
{
    return textureLod(sampler3D(tex, samp), texcoord, lod);
}

vec4 Texture3DTexel(sampler3D tex, ivec3 coord)
{
    return texelFetch(tex, coord, 0);
}

vec4 Texture3DTexel(sampler samp, texture3D tex, ivec3 coord)
{
    return texelFetch(sampler3D(tex, samp), coord, 0);
}

vec4 Texture3DTexelLod(sampler samp, texture3D tex, ivec3 coord, int lod)
{
    return texelFetch(sampler3D(tex, samp), coord, lod);
}

ivec2 TextureSize(sampler samp, texture2D tex)
{
    return textureSize(sampler2D(tex, samp), 0);
}

ivec3 TextureSize(sampler samp, texture3D tex)
{
    return textureSize(sampler3D(tex, samp), 0);
}

#ifndef HYP_NO_CUBEMAP

vec4 TextureCube(samplerCube tex, vec3 texcoord)
{
    return texture(tex, texcoord);
}

vec4 TextureCube(sampler samp, textureCube tex, vec3 texcoord)
{
    return texture(samplerCube(tex, samp), texcoord);
}

vec4 TextureCubeLod(sampler samp, textureCube tex, vec3 texcoord, float lod)
{
    return textureLod(samplerCube(tex, samp), texcoord, lod);
}

#endif

vec3 GetTriplanarBlend(vec3 normal)
{
    vec3 blending = normalize(max(abs(normal), 0.0001));
    blending /= (blending.x + blending.y + blending.z);

    return blending;
}

vec4 Texture2DTriplanar(sampler samp, texture2D tex, vec3 position, vec3 normal)
{
    vec3 blending = GetTriplanarBlend(normal);

    vec4 sample_x = Texture2D(samp, tex, position.zy * 0.01);
    vec4 sample_y = Texture2D(samp, tex, position.xz * 0.01);
    vec4 sample_z = Texture2D(samp, tex, position.xy * 0.01);

    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

vec4 Texture2DTriplanar(sampler2D tex, vec3 position, vec3 normal)
{
    vec3 blending = GetTriplanarBlend(normal);

    vec4 sample_x = Texture2D(tex, position.zy * 0.01);
    vec4 sample_y = Texture2D(tex, position.xz * 0.01);
    vec4 sample_z = Texture2D(tex, position.xy * 0.01);

    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

int GetNumLevels(sampler samp, texture2D tex)
{
    return textureQueryLevels(sampler2D(tex, samp));
}

int GetNumLevels(sampler samp, texture3D tex)
{
    return textureQueryLevels(sampler3D(tex, samp));
}

int GetNumLevels(sampler samp, textureCube tex)
{
    return textureQueryLevels(samplerCube(tex, samp));
}

#define GAUSS_TABLE_SIZE 15

const float gauss_table[GAUSS_TABLE_SIZE + 1] = float[](
    0.1847392078702266,
    0.16595854345772326,
    0.12031364177766891,
    0.07038755277896766,
    0.03322925565155569,
    0.012657819729901945,
    0.0038903040680094217,
    0.0009646503390864025,
    0.00019297087402915717,
    0.000031139936308099136,
    0.000004053309048174758,
    4.255228059965837e-7,
    3.602517634249573e-8,
    2.4592560765896795e-9,
    1.3534945386863618e-10,
    0.0);

float GaussianWeight(float value)
{
    float idxf;
    float c = modf(max(0.0, value * float(GAUSS_TABLE_SIZE)), idxf);
    int idx = int(idxf);
    if (idx >= GAUSS_TABLE_SIZE + 1)
    {
        return 0.0;
    }

    return mix(gauss_table[idx], gauss_table[idx + 1], c);
}

vec4 GaussianBlur9(
    texture2D tex, sampler samp,
    vec2 uv,
    vec2 direction,
    float radius)
{
    vec4 color = vec4(0.0);
    vec2 offset1 = vec2(1.3846153846) * direction;
    vec2 offset2 = vec2(3.2307692308) * direction;
    color += Texture2D(samp, tex, uv) * 0.2270270270;
    color += Texture2D(samp, tex, uv + (offset1 * radius)) * 0.3162162162;
    color += Texture2D(samp, tex, uv - (offset1 * radius)) * 0.3162162162;
    color += Texture2D(samp, tex, uv + (offset2 * radius)) * 0.0702702703;
    color += Texture2D(samp, tex, uv - (offset2 * radius)) * 0.0702702703;
    return color;
}

vec4 GaussianBlur5(
    texture2D tex, sampler samp,
    vec2 uv,
    vec2 direction,
    float radius)
{
    vec4 color = vec4(0.0);
    vec2 offset = vec2(1.3333333333333333) * direction;
    color += Texture2D(samp, tex, uv) * 0.29411764705882354;
    color += Texture2D(samp, tex, uv + (offset * radius)) * 0.35294117647058826;
    color += Texture2D(samp, tex, uv - (offset * radius)) * 0.35294117647058826;
    return color;
}

#endif

float LinearDepth(mat4 projection, float depth)
{
    return projection[3][2] / (depth * projection[2][3] - projection[2][2]);
}
float LinearizeDepth(float depth, float near, float far)
{
    return near * far / (far + depth * (near - far));
}

// Based on Unity's Linear01Depth function, Unity uses a reversed depth buffer though.
float Linear01Depth(float depth, float near, float far)
{
    float x = 1.0 - (far / near);
    float y = (far / near);
    float z = x / far;
    float w = y / far;

    return 1.0 - (1.0 / (x * (1.0 - depth) + y));
}

float ProjectedDepth(mat4 projection, float depth)
{
    return (projection * vec4(0.0, 0.0, depth, 1.0)).z;

    /*float result = depth;
    result *= projection[3][2];
    result += projection[2][2];
    result /= projection[2][3];

    return result;*/
}

float ViewDepth(float depth, float near, float far)
{
    float x = 1.0 - (far / near);
    float y = (far / near);
    float z = x / far;
    float w = y / far;

    return 1.0 / (z * depth + w);

    // return (far * near) / (far - depth * (far - near));
}

vec4 ReconstructWorldSpacePositionFromDepth(mat4 inverse_projection, mat4 inverse_view, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

    vec4 inversed = inverse_projection * ndc;
    inversed /= inversed.w;
    inversed = inverse_view * inversed;

    return inversed;
}

vec4 ReconstructViewSpacePositionFromDepth(mat4 inverse_projection, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

    vec4 inversed = inverse_projection * ndc;
    inversed /= inversed.w;

    return inversed;
}

vec2 GetProjectedPositionFromWorld(in mat4 projection, in mat4 view, in vec3 world_space_position)
{
    vec4 sample_position = projection * view * vec4(world_space_position, 1.0);
    return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

vec2 GetProjectedPositionFromWorld(in mat4 view_projection, in vec3 world_space_position)
{
    vec4 sample_position = view_projection * vec4(world_space_position, 1.0);
    return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

vec2 GetProjectedPositionFromView(in mat4 projection, in vec3 view_space_position)
{
    vec4 sample_position = projection * vec4(view_space_position, 1.0);
    return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

float Saturate(float val)
{
    return min(max(val, 0.0), 1.0);
}

vec2 Saturate(vec2 val)
{
    return min(max(val, vec2(0.0)), vec2(1.0));
}

vec3 Saturate(vec3 val)
{
    return min(max(val, vec3(0.0)), vec3(1.0));
}

vec4 Saturate(vec4 val)
{
    return min(max(val, vec4(0.0)), vec4(1.0));
}

vec4 CalculateFogExp(in vec4 start_color, in vec4 end_color, vec3 world_position, vec3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);
    const float density = 0.00003;

    const float fog_factor = 1.0 / exp(dist * density);

    return mix(start_color, end_color, 1.0 - fog_factor);
}

vec4 CalculateFogLinear(in vec4 start_color, in vec4 end_color, vec3 world_position, vec3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);

    const float fog_factor = Saturate((fog_end - dist) / (fog_end - fog_start));

    return mix(start_color, end_color, 1.0 - fog_factor);
}

float RoughnessToConeAngle(float roughness)
{
    float specular_power = 2.0 / pow(roughness, 4.0) - 2.0;

    const float xi = 0.244;
    float exponent = 1.0 / (specular_power + 1.0);
    return acos(pow(xi, exponent));
}

float linstep(float min, float max, float v)
{
    return clamp((v - min) / (max - min), 0.0, 1.0);
}

vec3 RGBToYCoCg(in vec3 rgb)
{
    float co = rgb.r - rgb.b;
    float t = rgb.b + co / 2.0;
    float cg = rgb.g - t;
    float y = t + cg / 2.0;
    return vec3(y, co, cg);
}

vec4 RGBToYCoCg(in vec4 rgb)
{
    float co = rgb.r - rgb.b;
    float t = rgb.b + co / 2.0;
    float cg = rgb.g - t;
    float y = t + cg / 2.0;
    return vec4(y, co, cg, rgb.a);
}

vec3 YCoCgToRGB(in vec3 ycocg)
{
    float t = ycocg.r - ycocg.b / 2.0;
    float g = ycocg.b + t;
    float b = t - ycocg.g / 2.0;
    float r = ycocg.g + b;
    return vec3(r, g, b);
}

vec4 YCoCgToRGB(in vec4 ycocg)
{
    float t = ycocg.r - ycocg.b / 2.0;
    float g = ycocg.b + t;
    float b = t - ycocg.g / 2.0;
    float r = ycocg.g + b;
    return vec4(r, g, b, ycocg.a);
}

//// Cubemap utilities

// pairs of cubemap forward direction and up direction (interleaved order)
const vec3 g_cubemapDirections[12] = vec3[](
    vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0),
    vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, -1.0), vec3(0.0, 1.0, 0.0));

vec3 GetCubemapCoord(uint face, vec2 uv)
{
    vec3 forward = g_cubemapDirections[face * 2];
    vec3 up = g_cubemapDirections[face * 2 + 1];
    vec3 right = cross(forward, up);

    vec2 coord = uv * 2.0 - 1.0;

    return normalize(forward - right * coord.x - up * coord.y);
}

uint GetCubemapFaceIndex(vec3 dir)
{
    uint face = 0u;
    float ax = abs(dir.x);
    float ay = abs(dir.y);
    float az = abs(dir.z);

    if (ay > ax && ay > az)
    {
        face = (dir.y < 0.0) ? 3u : 2u;
    }
    else if (az > ax && az > ay)
    {
        face = (dir.z < 0.0) ? 5u : 4u;
    }
    else
    {
        face = (dir.x < 0.0) ? 1u : 0u;
    }

    return face;
}

///// End cubemap utilities

#endif