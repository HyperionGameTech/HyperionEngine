#include "include/Defines.hlsli"

PERMUTE(INSTANCING);
PERMUTE(TERRAIN_MORPH);
PERMUTE(SHADING_TYPE, DEFERRED, FORWARD, LIGHTMAPPED, UNLIT);

#define TERRAIN_SPLAT_SCALE 0.002
#define TERRAIN_LAYER0_SCALE 0.08
#define TERRAIN_LAYER1_SCALE 0.045
#define TERRAIN_LAYER2_SCALE 0.06
#define TERRAIN_LAYER3_SCALE 0.10

#define TERRAIN_SLOPE_BLEND_START 0.25
#define TERRAIN_SLOPE_BLEND_END 0.55

#define TERRAIN_LAYER0_ROUGHNESS 0.90
#define TERRAIN_LAYER1_ROUGHNESS 0.85
#define TERRAIN_LAYER2_ROUGHNESS 0.85
#define TERRAIN_LAYER3_ROUGHNESS 0.60

#define TERRAIN_LAYER0_AO 1.00
#define TERRAIN_LAYER1_AO 0.82
#define TERRAIN_LAYER2_AO 0.90
#define TERRAIN_LAYER3_AO 1.00

#define TERRAIN_SPLAT_SHARPNESS 1.35
#define TERRAIN_SPLAT_NOISE_BREAKUP 0.10

#define TERRAIN_ANTITILE_ROTATION 1.1
#define TERRAIN_ANTITILE_MASK_SCALE 0.03

#define TERRAIN_MACRO_NOISE_SCALE 0.012
#define TERRAIN_MACRO_STRENGTH 0.12
#define TERRAIN_ROUGHNESS_NOISE_STRENGTH 0.08

#define TERRAIN_CAVITY_SLOPE_STRENGTH 0.35
#define TERRAIN_CAVITY_MACRO_STRENGTH 0.30
#define TERRAIN_CAVITY_DETAIL_SCALE 0.35
#define TERRAIN_CAVITY_DETAIL_STRENGTH 0.15
#define TERRAIN_ROUGHNESS_ALBEDO_STRENGTH 0.35
#define TERRAIN_ROUGHNESS_SLOPE_STRENGTH 0.15

#define TERRAIN_PACKED_AO_STRENGTH 1.0
// a layer's height rides on its splat weight - only layers within this depth of the tallest one show through
#define TERRAIN_HEIGHT_BLEND_DEPTH 0.1
// how far a layer's height map can lift it over layers with more splat weight
#define TERRAIN_HEIGHT_BLEND_STRENGTH 1.0
// height used for layers without a height map
#define TERRAIN_DEFAULT_LAYER_HEIGHT 0.5

// concavity (normal map alpha) - erosion hollows are darker and duller, ridges catch the light
#define TERRAIN_HOLLOW_DARKEN 0.35
#define TERRAIN_HOLLOW_DESATURATE 0.3
#define TERRAIN_RIDGE_LIGHTEN 0.15

// world units at height 0; heights are normalized to 0..1 at cook time
#define TERRAIN_LAYER0_PARALLAX_DEPTH 0.12
#define TERRAIN_LAYER1_PARALLAX_DEPTH 0.20
#define TERRAIN_LAYER2_PARALLAX_DEPTH 0.15
#define TERRAIN_LAYER3_PARALLAX_DEPTH 0.10

#define TERRAIN_PARALLAX_MIN_STEPS 6
#define TERRAIN_PARALLAX_MAX_STEPS 16
#define TERRAIN_PARALLAX_MIN_VIEW_COSINE 0.25
#define TERRAIN_PARALLAX_FADE_START 15.0
#define TERRAIN_PARALLAX_FADE_END 40.0

#define TERRAIN_NORMAL_STRENGTH 1.6
#define TERRAIN_NORMAL_FADE_START 40.0
#define TERRAIN_NORMAL_FADE_END 250.0

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float4 color : TEXCOORD2;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#include "include/Material.hlsli"

#define MATERIAL_TEXTURE_TerrainSplatMap 6
#define MATERIAL_TEXTURE_TerrainLayer0 7
#define MATERIAL_TEXTURE_TerrainLayer1 8
#define MATERIAL_TEXTURE_TerrainLayer2 9
#define MATERIAL_TEXTURE_TerrainLayer3 10
#define MATERIAL_TEXTURE_TerrainNormal0 11
#define MATERIAL_TEXTURE_TerrainNormal1 12
#define MATERIAL_TEXTURE_TerrainNormal2 13
#define MATERIAL_TEXTURE_TerrainNormal3 14
#define MATERIAL_TEXTURE_TerrainNormalMap 15

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Material, TerrainSplatMap) Texture2D TerrainSplatMap;
DECLARE_SRV(Material, TerrainLayer0) Texture2D TerrainLayer0;
DECLARE_SRV(Material, TerrainLayer1) Texture2D TerrainLayer1;
DECLARE_SRV(Material, TerrainLayer2) Texture2D TerrainLayer2;
DECLARE_SRV(Material, TerrainLayer3) Texture2D TerrainLayer3;
DECLARE_SRV(Material, TerrainNormal0) Texture2D TerrainNormal0;
DECLARE_SRV(Material, TerrainNormal1) Texture2D TerrainNormal1;
DECLARE_SRV(Material, TerrainNormal2) Texture2D TerrainNormal2;
DECLARE_SRV(Material, TerrainNormal3) Texture2D TerrainNormal3;
DECLARE_SRV(Material, TerrainNormalMap) Texture2D TerrainNormalMap;
#endif // !HYP_FEATURES_BINDLESS_TEXTURES

#include "include/Scene.hlsli"
#include "include/Packing.hlsli"
#include "include/EnvProbes.hlsli"
#include "include/Gbuffer.hlsli"
#include "include/Entity.hlsli"

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
    float4x4 vpMatrix;
};

float TerrainValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float TerrainFbm(float2 p)
{
    float value = 0.0;
    float amplitude = 0.5;

    for (int octave = 0; octave < 4; octave++)
    {
        value += amplitude * TerrainValueNoise(p);
        p = p * 2.07 + float2(13.1, 5.7);
        amplitude *= 0.5;
    }

    return value;
}

float2 RotateUv(float2 uv, float angle)
{
    const float s = sin(angle);
    const float c = cos(angle);

    return float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
}

float3 GetTerrainNormalBlending(float3 normal)
{
    float3 blending = pow(abs(normal), 4.0);
    return blending / max(blending.x + blending.y + blending.z, 0.0001);
}

float4 BlendTriplanar(float4 sample_x, float4 sample_y, float4 sample_z, float3 blending)
{
    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

// derivatives come from the unshifted position so parallax step boundaries don't pop the mip level
struct TerrainTriplanarCoords
{
    float2 uv_x;
    float2 uv_y;
    float2 uv_z;
    float2 uv_ddx_x;
    float2 uv_ddx_y;
    float2 uv_ddx_z;
    float2 uv_ddy_x;
    float2 uv_ddy_y;
    float2 uv_ddy_z;
};

TerrainTriplanarCoords MakeTerrainTriplanarCoords(float3 position, float3 position_ddx, float3 position_ddy, float scale)
{
    TerrainTriplanarCoords coords;

    coords.uv_x = position.zy * scale;
    coords.uv_y = position.xz * scale;
    coords.uv_z = position.xy * scale;
    coords.uv_ddx_x = position_ddx.zy * scale;
    coords.uv_ddx_y = position_ddx.xz * scale;
    coords.uv_ddx_z = position_ddx.xy * scale;
    coords.uv_ddy_x = position_ddy.zy * scale;
    coords.uv_ddy_y = position_ddy.xz * scale;
    coords.uv_ddy_z = position_ddy.xy * scale;

    return coords;
}

TerrainTriplanarCoords RotateTerrainTriplanarCoords(TerrainTriplanarCoords coords, float angle, float2 offset)
{
    TerrainTriplanarCoords rotated;

    rotated.uv_x = RotateUv(coords.uv_x, angle) + offset;
    rotated.uv_y = RotateUv(coords.uv_y, angle) + offset;
    rotated.uv_z = RotateUv(coords.uv_z, angle) + offset;
    rotated.uv_ddx_x = RotateUv(coords.uv_ddx_x, angle);
    rotated.uv_ddx_y = RotateUv(coords.uv_ddx_y, angle);
    rotated.uv_ddx_z = RotateUv(coords.uv_ddx_z, angle);
    rotated.uv_ddy_x = RotateUv(coords.uv_ddy_x, angle);
    rotated.uv_ddy_y = RotateUv(coords.uv_ddy_y, angle);
    rotated.uv_ddy_z = RotateUv(coords.uv_ddy_z, angle);

    return rotated;
}

float4 SampleTriplanarGrad(Texture2D tex, TerrainTriplanarCoords coords, float3 blending)
{
    return BlendTriplanar(
        SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_x, coords.uv_ddx_x, coords.uv_ddy_x),
        SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_y, coords.uv_ddx_y, coords.uv_ddy_y),
        SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_z, coords.uv_ddx_z, coords.uv_ddy_z),
        blending);
}

float4 SampleTriplanarAntiTile(Texture2D tex, TerrainTriplanarCoords coords, float3 blending, float antitile_mask)
{
    const float2 alt_offset = float2(0.37, 0.71);

    return lerp(
        SampleTriplanarGrad(tex, coords, blending),
        SampleTriplanarGrad(tex, RotateTerrainTriplanarCoords(coords, TERRAIN_ANTITILE_ROTATION, alt_offset), blending),
        antitile_mask);
}

float4 SampleTerrainLayer(uint layerIndex, float3 position, float3 position_ddx, float3 position_ddy, float3 blending, float antitile_mask)
{
    Texture2D tex;
    float scale;

    switch (layerIndex)
    {
    case 0: tex = GET_TEXTURE(material, TerrainLayer0); scale = TERRAIN_LAYER0_SCALE; break;
    case 1: tex = GET_TEXTURE(material, TerrainLayer1); scale = TERRAIN_LAYER1_SCALE; break;
    case 2: tex = GET_TEXTURE(material, TerrainLayer2); scale = TERRAIN_LAYER2_SCALE; break;
    default: tex = GET_TEXTURE(material, TerrainLayer3); scale = TERRAIN_LAYER3_SCALE; break;
    }

    return SampleTriplanarAntiTile(tex, MakeTerrainTriplanarCoords(position, position_ddx, position_ddy, scale), blending, antitile_mask);
}

float3 SampleTerrainLayerNormal(uint layerIndex, float3 position, float3 position_ddx, float3 position_ddy, float3 blending)
{
    Texture2D tex;
    float scale;

    switch (layerIndex)
    {
    case 0: tex = GET_TEXTURE(material, TerrainNormal0); scale = TERRAIN_LAYER0_SCALE; break;
    case 1: tex = GET_TEXTURE(material, TerrainNormal1); scale = TERRAIN_LAYER1_SCALE; break;
    case 2: tex = GET_TEXTURE(material, TerrainNormal2); scale = TERRAIN_LAYER2_SCALE; break;
    default: tex = GET_TEXTURE(material, TerrainNormal3); scale = TERRAIN_LAYER3_SCALE; break;
    }

    const TerrainTriplanarCoords coords = MakeTerrainTriplanarCoords(position, position_ddx, position_ddy, scale);

    float3 tangent_normal_x = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_x, coords.uv_ddx_x, coords.uv_ddy_x).rgb * 2.0 - 1.0;
    float3 tangent_normal_y = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_y, coords.uv_ddx_y, coords.uv_ddy_y).rgb * 2.0 - 1.0;
    float3 tangent_normal_z = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_z, coords.uv_ddx_z, coords.uv_ddy_z).rgb * 2.0 - 1.0;

    if (GET_MATERIAL_PARAM_BIT(material, MATERIAL_FLAG_NORMAL_MAP_FLIP_Y))
    {
        tangent_normal_x.y = -tangent_normal_x.y;
        tangent_normal_y.y = -tangent_normal_y.y;
        tangent_normal_z.y = -tangent_normal_z.y;
    }

    return normalize(
        tangent_normal_x.zyx * blending.x
        + tangent_normal_y.xzy * blending.y
        + tangent_normal_z.xyz * blending.z);
}

float SampleTerrainLayerHeight(uint layerIndex, float3 position, float3 position_ddx, float3 position_ddy, float3 blending)
{
    Texture2D tex;
    float scale;

    switch (layerIndex)
    {
    case 0: tex = GET_TEXTURE(material, TerrainNormal0); scale = TERRAIN_LAYER0_SCALE; break;
    case 1: tex = GET_TEXTURE(material, TerrainNormal1); scale = TERRAIN_LAYER1_SCALE; break;
    case 2: tex = GET_TEXTURE(material, TerrainNormal2); scale = TERRAIN_LAYER2_SCALE; break;
    default: tex = GET_TEXTURE(material, TerrainNormal3); scale = TERRAIN_LAYER3_SCALE; break;
    }

    return SampleTriplanarGrad(tex, MakeTerrainTriplanarCoords(position, position_ddx, position_ddy, scale), blending).a;
}

float SampleTerrainSurfaceDepth(float3 position, float3 position_ddx, float3 position_ddy, float3 blending, float4 weights)
{
    float depth = 0.0;

    if (weights.x > 0.001 && HAS_TEXTURE(material, TerrainNormal0))
        depth += weights.x * (1.0 - SampleTerrainLayerHeight(0, position, position_ddx, position_ddy, blending)) * TERRAIN_LAYER0_PARALLAX_DEPTH;
    if (weights.y > 0.001 && HAS_TEXTURE(material, TerrainNormal1))
        depth += weights.y * (1.0 - SampleTerrainLayerHeight(1, position, position_ddx, position_ddy, blending)) * TERRAIN_LAYER1_PARALLAX_DEPTH;
    if (weights.z > 0.001 && HAS_TEXTURE(material, TerrainNormal2))
        depth += weights.z * (1.0 - SampleTerrainLayerHeight(2, position, position_ddx, position_ddy, blending)) * TERRAIN_LAYER2_PARALLAX_DEPTH;
    if (weights.w > 0.001 && HAS_TEXTURE(material, TerrainNormal3))
        depth += weights.w * (1.0 - SampleTerrainLayerHeight(3, position, position_ddx, position_ddy, blending)) * TERRAIN_LAYER3_PARALLAX_DEPTH;

    return depth;
}

// marched in world space along the surface plane, so every triplanar projection and layer scale shares one offset
float3 GetTerrainParallaxOffset(float3 position, float3 position_ddx, float3 position_ddy, float3 normal, float3 view_direction, float3 blending, float4 weights, float fade)
{
    const float max_depth = dot(weights, float4(TERRAIN_LAYER0_PARALLAX_DEPTH, TERRAIN_LAYER1_PARALLAX_DEPTH, TERRAIN_LAYER2_PARALLAX_DEPTH, TERRAIN_LAYER3_PARALLAX_DEPTH)) * fade;

    if (max_depth < 0.0001)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float normal_dot_view = dot(normal, view_direction);
    const float clamped_normal_dot_view = max(normal_dot_view, TERRAIN_PARALLAX_MIN_VIEW_COSINE);

    const float3 shift_per_depth = -(view_direction - normal * normal_dot_view) / clamped_normal_dot_view;

    const int num_steps = int(lerp(float(TERRAIN_PARALLAX_MAX_STEPS), float(TERRAIN_PARALLAX_MIN_STEPS), saturate(normal_dot_view)));
    const float step_depth = max_depth / float(num_steps);

    float ray_depth = 0.0;
    float surface_depth = SampleTerrainSurfaceDepth(position, position_ddx, position_ddy, blending, weights) * fade;

    float previous_ray_depth = ray_depth;
    float previous_surface_depth = surface_depth;

    [loop]
    for (int step_index = 0; step_index < num_steps && ray_depth < surface_depth; step_index++)
    {
        previous_ray_depth = ray_depth;
        previous_surface_depth = surface_depth;

        ray_depth += step_depth;
        surface_depth = SampleTerrainSurfaceDepth(position + shift_per_depth * ray_depth, position_ddx, position_ddy, blending, weights) * fade;
    }

    const float depth_above_before = previous_surface_depth - previous_ray_depth;
    const float depth_above_after = surface_depth - ray_depth;
    const float denominator = depth_above_before - depth_above_after;

    const float hit_depth = denominator > 0.0001
        ? lerp(previous_ray_depth, ray_depth, saturate(depth_above_before / denominator))
        : ray_depth;

    return shift_per_depth * hit_depth;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float3 P = input.position.xyz;
    const float3 position_ddx = ddx(P);
    const float3 position_ddy = ddy(P);

    // full resolution normals per cell, so shading doesn't change when the mesh LOD underneath it does
    float3 N;

    // -1 ridge to 1 hollow
    float concavity = 0.0;

    if (HAS_TEXTURE(material, TerrainNormalMap))
    {
        const float4 normal_map_sample = SAMPLE_TEXTURE_2D(texture_sampler, GET_TEXTURE(material, TerrainNormalMap), input.texcoord0);

        N = normalize(normal_map_sample.xyz * 2.0 - 1.0);
        concavity = normal_map_sample.a * 2.0 - 1.0;
    }
    else
    {
        N = normalize(input.normal);
    }

    const float3 blending = GetTriplanarBlend(N);
    const float3 normal_blending = GetTerrainNormalBlending(N);

    const float view_distance = length(P - input.camera_position);
    const float detail_fade = 1.0 - smoothstep(TERRAIN_NORMAL_FADE_START, TERRAIN_NORMAL_FADE_END, view_distance);

    const float macro_noise = TerrainFbm(P.xz * TERRAIN_MACRO_NOISE_SCALE);
    const float antitile_mask = smoothstep(0.35, 0.65, TerrainFbm(P.xz * TERRAIN_ANTITILE_MASK_SCALE + 43.7));

    const float slope = saturate(1.0 - N.y);

    float4 weights;

    if (HAS_TEXTURE(material, TerrainSplatMap))
    {
        weights = SAMPLE_TEXTURE_2D(texture_sampler, GET_TEXTURE(material, TerrainSplatMap), input.texcoord0);

        const float splat_noise = TerrainValueNoise(P.xz * 0.035) - 0.5;
        weights = saturate((weights - 0.5) * TERRAIN_SPLAT_SHARPNESS + 0.5 + splat_noise * TERRAIN_SPLAT_NOISE_BREAKUP);
    }
    else
    {
        const float threshold_jitter = (TerrainValueNoise(P.xz * 0.015) - 0.5) * 0.3;

        const float rock_blend = smoothstep(
            TERRAIN_SLOPE_BLEND_START + threshold_jitter,
            TERRAIN_SLOPE_BLEND_END + threshold_jitter,
            slope);

        weights = float4(1.0 - rock_blend, rock_blend, 0.0, 0.0);
    }
    
    weights.x *= float(HAS_TEXTURE(material, TerrainLayer0));
    weights.y *= float(HAS_TEXTURE(material, TerrainLayer1));
    weights.z *= float(HAS_TEXTURE(material, TerrainLayer2));
    weights.w *= float(HAS_TEXTURE(material, TerrainLayer3));

    const float base_total_weight = weights.x + weights.y + weights.z + weights.w;
    const float parallax_fade = 1.0 - smoothstep(TERRAIN_PARALLAX_FADE_START, TERRAIN_PARALLAX_FADE_END, view_distance);

    float3 parallax_offset = float3(0.0, 0.0, 0.0);

    if (base_total_weight > 0.0001 && parallax_fade > 0.001)
    {
        const float3 view_direction = (input.camera_position - P) / max(view_distance, 0.0001);

        parallax_offset = GetTerrainParallaxOffset(P, position_ddx, position_ddy, N, view_direction, blending, weights / base_total_weight, parallax_fade);
    }

    const float3 detail_position = P + parallax_offset;

    // stones poke out of the grass around them instead of fading into it; layers with no weight never show
    float4 layer_heights = float4(-1.0, -1.0, -1.0, -1.0);
    float4 layer_detail_heights = float4(TERRAIN_DEFAULT_LAYER_HEIGHT, TERRAIN_DEFAULT_LAYER_HEIGHT, TERRAIN_DEFAULT_LAYER_HEIGHT, TERRAIN_DEFAULT_LAYER_HEIGHT);

    if (weights.x > 0.001 && HAS_TEXTURE(material, TerrainNormal0))
        layer_detail_heights.x = SampleTerrainLayerHeight(0, detail_position, position_ddx, position_ddy, blending);
    if (weights.y > 0.001 && HAS_TEXTURE(material, TerrainNormal1))
        layer_detail_heights.y = SampleTerrainLayerHeight(1, detail_position, position_ddx, position_ddy, blending);
    if (weights.z > 0.001 && HAS_TEXTURE(material, TerrainNormal2))
        layer_detail_heights.z = SampleTerrainLayerHeight(2, detail_position, position_ddx, position_ddy, blending);
    if (weights.w > 0.001 && HAS_TEXTURE(material, TerrainNormal3))
        layer_detail_heights.w = SampleTerrainLayerHeight(3, detail_position, position_ddx, position_ddy, blending);

    const float4 has_weight = step(0.001, weights);
    layer_heights = lerp(layer_heights, weights + layer_detail_heights * TERRAIN_HEIGHT_BLEND_STRENGTH, has_weight);

    const float blend_threshold = max(max(layer_heights.x, layer_heights.y), max(layer_heights.z, layer_heights.w)) - TERRAIN_HEIGHT_BLEND_DEPTH;

    weights = max(layer_heights - blend_threshold, 0.0) * has_weight;

    const float total_weight = weights.x + weights.y + weights.z + weights.w;

    const bool any_layers = total_weight > 0.0001;

    if (any_layers)
    {
        weights /= total_weight;
    }

    float3 albedo = material.albedo.rgb;
    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);
    const float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);
    
    float packed_ao = 0.0;

    float3 blended_normal = float3(0.0, 0.0, 0.0);
    float total_normal_weight = 0.0;

    if (any_layers)
    {
        albedo = float3(0.0, 0.0, 0.0);
        roughness = 0.0;

        if (weights.x > 0.001)
        {
            const float4 layer0_sample = SampleTerrainLayer(0, detail_position, position_ddx, position_ddy, blending, antitile_mask);
            albedo += weights.x * layer0_sample.rgb;
            packed_ao += weights.x * layer0_sample.a;
            roughness += weights.x * TERRAIN_LAYER0_ROUGHNESS;

            if (HAS_TEXTURE(material, TerrainNormal0))
            {
                blended_normal += weights.x * SampleTerrainLayerNormal(0, detail_position, position_ddx, position_ddy, normal_blending);
                total_normal_weight += weights.x;
            }
        }

        if (weights.y > 0.001)
        {
            const float4 layer1_sample = SampleTerrainLayer(1, detail_position, position_ddx, position_ddy, blending, antitile_mask);
            albedo += weights.y * layer1_sample.rgb;
            packed_ao += weights.y * layer1_sample.a;
            roughness += weights.y * TERRAIN_LAYER1_ROUGHNESS;

            if (HAS_TEXTURE(material, TerrainNormal1))
            {
                blended_normal += weights.y * SampleTerrainLayerNormal(1, detail_position, position_ddx, position_ddy, normal_blending);
                total_normal_weight += weights.y;
            }
        }

        if (weights.z > 0.001)
        {
            const float4 layer2_sample = SampleTerrainLayer(2, detail_position, position_ddx, position_ddy, blending, antitile_mask);
            albedo += weights.z * layer2_sample.rgb;
            packed_ao += weights.z * layer2_sample.a;
            roughness += weights.z * TERRAIN_LAYER2_ROUGHNESS;

            if (HAS_TEXTURE(material, TerrainNormal2))
            {
                blended_normal += weights.z * SampleTerrainLayerNormal(2, detail_position, position_ddx, position_ddy, normal_blending);
                total_normal_weight += weights.z;
            }
        }

        if (weights.w > 0.001)
        {
            const float4 layer3_sample = SampleTerrainLayer(3, detail_position, position_ddx, position_ddy, blending, antitile_mask);
            albedo += weights.w * layer3_sample.rgb;
            packed_ao += weights.w * layer3_sample.a;
            roughness += weights.w * TERRAIN_LAYER3_ROUGHNESS;

            if (HAS_TEXTURE(material, TerrainNormal3))
            {
                blended_normal += weights.w * SampleTerrainLayerNormal(3, detail_position, position_ddx, position_ddy, normal_blending);
                total_normal_weight += weights.w;
            }
        }
        if (total_normal_weight > 0.001 && detail_fade > 0.001)
        {
            N = normalize(blended_normal * (TERRAIN_NORMAL_STRENGTH * detail_fade) + N);
        }
    }
    
    // if (any_layers)
    //     albedo *= lerp(1.0, packed_ao, TERRAIN_PACKED_AO_STRENGTH);

    albedo *= lerp(1.0 - TERRAIN_MACRO_STRENGTH, 1.0 + TERRAIN_MACRO_STRENGTH, macro_noise);

    const float hollow = saturate(concavity);
    const float ridge = saturate(-concavity);

    const float albedo_luminance = dot(albedo, float3(0.2126, 0.7152, 0.0722));

    albedo = lerp(albedo, float3(albedo_luminance, albedo_luminance, albedo_luminance), hollow * TERRAIN_HOLLOW_DESATURATE);
    albedo *= (1.0 - hollow * TERRAIN_HOLLOW_DARKEN) * (1.0 + ridge * TERRAIN_RIDGE_LIGHTEN);

    // float layer_ao = any_layers
    //     ? (weights.x * TERRAIN_LAYER0_AO
    //         + weights.y * TERRAIN_LAYER1_AO
    //         + weights.z * TERRAIN_LAYER2_AO
    //         + weights.w * TERRAIN_LAYER3_AO)
    //     : 1.0;
    // const float slope_ao = 1.0 - TERRAIN_CAVITY_SLOPE_STRENGTH * smoothstep(0.0, 0.6, slope);
    // const float macro_ao = lerp(1.0 - TERRAIN_CAVITY_MACRO_STRENGTH, 1.0, macro_noise);
    // const float detail_noise = TerrainValueNoise(P.xz * TERRAIN_CAVITY_DETAIL_SCALE);
    // const float detail_ao = 1.0 - TERRAIN_CAVITY_DETAIL_STRENGTH * (1.0 - detail_noise);
    // albedo *= layer_ao * slope_ao * macro_ao * detail_ao;

    // const float albedo_luma = dot(albedo, float3(0.2126, 0.7152, 0.0722));
    // roughness = saturate(roughness
    //     + (albedo_luma - 0.25) * TERRAIN_ROUGHNESS_ALBEDO_STRENGTH
    //     + slope * TERRAIN_ROUGHNESS_SLOPE_STRENGTH
    //     + (detail_noise - 0.5) * TERRAIN_ROUGHNESS_NOISE_STRENGTH * 2.0);
    roughness = saturate(roughness * (1.0 + (macro_noise - 0.5) * (TERRAIN_ROUGHNESS_NOISE_STRENGTH * 2.0)));

    output.gbuffer_albedo = float4(albedo, 1.0);

    roughness = roughness * roughness;

    float2 velocity = float2(
        ((input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5)
            - ((input.previous_position_ndc.xy / input.previous_position_ndc.w) * 0.5 + 0.5));

    uint mask = input.object_mask;

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = metalness;
    materialParams.mask = mask;

    output.gbuffer_normals = GBufferPackNormal(N);

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);

    output.gbuffer_normals.x = roughnessAndMetalPacked;

    output.gbuffer_material = 0;

    // Mask is stored in the upper 4 bits of gbuffer_material
    output.gbuffer_material |= (maskPacked << 28u);

    output.gbuffer_velocity = velocity;

    return output;
}
