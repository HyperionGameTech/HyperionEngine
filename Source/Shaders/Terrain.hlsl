#include "include/Defines.hlsli"

PERMUTE(INSTANCING);
PERMUTE(TERRAIN_MORPH);
PERMUTE(SHADING_TYPE, DEFERRED, FORWARD, LIGHTMAPPED, UNLIT);

#define TERRAIN_SPLAT_SCALE 0.002

// cooked roughness is per-texel; these remap each layer's 0..1 range so a source map can be retuned without a recook
#define TERRAIN_LAYER0_ROUGHNESS_RANGE float2(0.78, 1.00)
#define TERRAIN_LAYER1_ROUGHNESS_RANGE float2(0.45, 0.92)
#define TERRAIN_LAYER2_ROUGHNESS_RANGE float2(0.62, 0.98)
// snow is diffuse dominant - only a wind crust is glossy, and a tight lobe on a near-white surface reads as a blown highlight
#define TERRAIN_LAYER3_ROUGHNESS_RANGE float2(0.55, 0.88)

#define TERRAIN_SPLAT_NOISE_BREAKUP 0.10

#define TERRAIN_ANTITILE_ROTATION 1.1
#define TERRAIN_ANTITILE_MASK_SCALE 0.03

// two octaves: ridge-scale patchiness and a much broader drift that survives to the horizon
#define TERRAIN_MACRO_NOISE_SCALE 0.012
#define TERRAIN_MACRO_STRENGTH 0.12
#define TERRAIN_FAR_NOISE_SCALE 0.0016
#define TERRAIN_FAR_STRENGTH 0.14

// No natural ground reflects more than this. The gbuffer albedo target is RGBA16F and does not clamp,
// so without a ceiling the macro gains multiply bright materials past 1.0 and snow turns into a flat white cutout.
#define TERRAIN_MAX_ALBEDO 0.88
// macro variation shifts hue as well as brightness; varying brightness alone still reads as one material
#define TERRAIN_MACRO_HUE_WARM float3(1.06, 1.00, 0.90)
#define TERRAIN_MACRO_HUE_COOL float3(0.92, 0.97, 1.08)

#define TERRAIN_ROUGHNESS_NOISE_STRENGTH 0.08

#define TERRAIN_PACKED_AO_STRENGTH 1.0
// material AO rides in gbuffer albedo alpha and only attenuates indirect light, so it can be applied at full strength
#define TERRAIN_AO_SLOPE_STRENGTH 0.25
#define TERRAIN_AO_HOLLOW_STRENGTH 0.30
#define TERRAIN_AO_MIN 0.25
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
#define TERRAIN_NORMAL_FADE_START 120.0
#define TERRAIN_NORMAL_FADE_END 1600.0
// detail never fades out entirely, otherwise distant slopes collapse to the interpolated mesh normal
#define TERRAIN_NORMAL_MIN_FADE 0.35

// Past the detail range the layer tiling drops below a texel and mips average it to a flat colour.
// The dominant layer is re-sampled at a much larger tiling that stays resolvable, and crossfaded in.
#define TERRAIN_MACRO_TILING_RATIO 0.14
#define TERRAIN_MACRO_TILING_FADE_START 150.0
#define TERRAIN_MACRO_TILING_FADE_END 700.0
#define TERRAIN_MACRO_TILING_MAX 0.75

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

#include "include/TerrainMaterial.hlsli"

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

// Textures are passed in rather than selected from a layer index: DXC cannot emit an OpPhi of an image type,
// so a Texture2D assigned across control flow fails SPIR-V validation unless the index folds to a constant.

// rg = tangent normal xy (z reconstructed), b = ambient occlusion, a = height
// Whiteout blend (add xy, multiply z), applied per triplanar projection with the geometric normal as the base.
// From the AMD "Ruby: Whiteout" demo, via Oat, C., "Real-Time Wrinkles", Advanced Real-Time Rendering in 3D
// Graphics and Games, SIGGRAPH Course, 2007. Named and compared against the alternatives in
// Barré-Brisebois, C. and Hill, S., "Blending in Detail", 2012:
// https://blog.selfshadow.com/publications/blending-in-detail/
//
// Folding the geometric normal in is what preserves its sign. A bare swizzle sends a flat detail normal to
// normalize(blendWeights), always in the +++ octant, so faces with a negative component shade in the wrong
// hemisphere and light up as if they faced the sun. Strength scales the tangent xy, so 0 returns N exactly.
float3 SampleTerrainNormalTriplanar(Texture2D tex, float3 position, float3 position_ddx, float3 position_ddy, float3 geometric_normal, float3 normal_blending, float3 blending, float scale, float strength, out float ao)
{
    const TerrainTriplanarCoords coords = MakeTerrainTriplanarCoords(position, position_ddx, position_ddy, scale);

    const float4 packed_x = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_x, coords.uv_ddx_x, coords.uv_ddy_x);
    const float4 packed_y = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_y, coords.uv_ddx_y, coords.uv_ddy_y);
    const float4 packed_z = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, coords.uv_z, coords.uv_ddx_z, coords.uv_ddy_z);

    ao = packed_x.b * blending.x + packed_y.b * blending.y + packed_z.b * blending.z;

    float3 tangent_normal_x = float3(packed_x.rg * 2.0 - 1.0, 0.0);
    float3 tangent_normal_y = float3(packed_y.rg * 2.0 - 1.0, 0.0);
    float3 tangent_normal_z = float3(packed_z.rg * 2.0 - 1.0, 0.0);

    tangent_normal_x.z = sqrt(saturate(1.0 - dot(tangent_normal_x.xy, tangent_normal_x.xy)));
    tangent_normal_y.z = sqrt(saturate(1.0 - dot(tangent_normal_y.xy, tangent_normal_y.xy)));
    tangent_normal_z.z = sqrt(saturate(1.0 - dot(tangent_normal_z.xy, tangent_normal_z.xy)));

    if (GET_MATERIAL_PARAM_BIT(material, MATERIAL_FLAG_NORMAL_MAP_FLIP_Y))
    {
        tangent_normal_x.y = -tangent_normal_x.y;
        tangent_normal_y.y = -tangent_normal_y.y;
        tangent_normal_z.y = -tangent_normal_z.y;
    }

    tangent_normal_x.xy *= strength;
    tangent_normal_y.xy *= strength;
    tangent_normal_z.xy *= strength;

    const float3 whiteout_x = float3(tangent_normal_x.xy + geometric_normal.zy, abs(tangent_normal_x.z) * geometric_normal.x);
    const float3 whiteout_y = float3(tangent_normal_y.xy + geometric_normal.xz, abs(tangent_normal_y.z) * geometric_normal.y);
    const float3 whiteout_z = float3(tangent_normal_z.xy + geometric_normal.xy, abs(tangent_normal_z.z) * geometric_normal.z);

    return normalize(
        whiteout_x.zyx * normal_blending.x
        + whiteout_y.xzy * normal_blending.y
        + whiteout_z.xyz * normal_blending.z);
}

float SampleTerrainHeightTriplanar(Texture2D tex, float3 position, float3 position_ddx, float3 position_ddy, float3 blending, float scale)
{
    return SampleTriplanarGrad(tex, MakeTerrainTriplanarCoords(position, position_ddx, position_ddy, scale), blending).a;
}

struct TerrainLayerContext
{
    float3 geometric_normal;
    float normal_strength;
    float3 detail_position;
    float3 position_ddx;
    float3 position_ddy;
    float3 blending;
    float3 normal_blending;
    float antitile_mask;
};

struct TerrainLayerAccumulator
{
    float3 albedo;
    float roughness;
    float ao;
    float3 normal;
    float normal_weight;
};

void AccumulateTerrainLayer(
    Texture2D albedoTexture,
    Texture2D normalTexture,
    bool hasNormalTexture,
    float weight,
    float scale,
    float3 tint,
    float2 roughnessRange,
    float farBlend,
    TerrainLayerContext context,
    inout TerrainLayerAccumulator accumulator)
{
    if (weight <= 0.001)
    {
        return;
    }

    float4 layer_sample = SampleTriplanarAntiTile(
        albedoTexture,
        MakeTerrainTriplanarCoords(context.detail_position, context.position_ddx, context.position_ddy, scale),
        context.blending,
        context.antitile_mask);

    if (farBlend > 0.001)
    {
        const float4 far_sample = SampleTriplanarGrad(
            albedoTexture,
            MakeTerrainTriplanarCoords(context.detail_position, context.position_ddx, context.position_ddy, scale * TERRAIN_MACRO_TILING_RATIO),
            context.blending);

        layer_sample = lerp(layer_sample, far_sample, farBlend);
    }

    accumulator.albedo += weight * layer_sample.rgb * tint;
    accumulator.roughness += weight * lerp(roughnessRange.x, roughnessRange.y, layer_sample.a);

    if (!hasNormalTexture)
    {
        accumulator.ao += weight;

        return;
    }

    float layer_ao;
    float3 layer_normal = SampleTerrainNormalTriplanar(
        normalTexture, context.detail_position, context.position_ddx, context.position_ddy,
        context.geometric_normal, context.normal_blending, context.blending, scale, context.normal_strength, layer_ao);

    if (farBlend > 0.001)
    {
        float far_ao;
        const float3 far_normal = SampleTerrainNormalTriplanar(
            normalTexture, context.detail_position, context.position_ddx, context.position_ddy,
            context.geometric_normal, context.normal_blending, context.blending, scale * TERRAIN_MACRO_TILING_RATIO, context.normal_strength, far_ao);

        layer_normal = normalize(lerp(layer_normal, far_normal, farBlend));
        layer_ao = lerp(layer_ao, far_ao, farBlend);
    }

    accumulator.normal += weight * layer_normal;
    accumulator.ao += weight * layer_ao;
    accumulator.normal_weight += weight;
}

// the layer index has to reach GET_TEXTURE as a literal, so this is a macro rather than a loop
#define ACCUMULATE_TERRAIN_LAYER(layerIndex, weight, dominantLayer, macroTilingFade, context, accumulator) \
    AccumulateTerrainLayer( \
        GET_TEXTURE(material, TerrainLayer##layerIndex), \
        GET_TEXTURE(material, TerrainNormal##layerIndex), \
        HAS_TEXTURE(material, TerrainNormal##layerIndex), \
        (weight), \
        TERRAIN_LAYER##layerIndex##_SCALE, \
        TERRAIN_LAYER##layerIndex##_TINT, \
        TERRAIN_LAYER##layerIndex##_ROUGHNESS_RANGE, \
        ((dominantLayer) == layerIndex##u) ? (macroTilingFade) : 0.0, \
        (context), \
        (accumulator))

#define SAMPLE_TERRAIN_LAYER_HEIGHT(layerIndex, position, blending) \
    SampleTerrainHeightTriplanar(GET_TEXTURE(material, TerrainNormal##layerIndex), (position), position_ddx, position_ddy, (blending), TERRAIN_LAYER##layerIndex##_SCALE)

float SampleTerrainSurfaceDepth(float3 position, float3 position_ddx, float3 position_ddy, float3 blending, float4 weights)
{
    float depth = 0.0;

    if (weights.x > 0.001 && HAS_TEXTURE(material, TerrainNormal0))
        depth += weights.x * (1.0 - SAMPLE_TERRAIN_LAYER_HEIGHT(0, position, blending)) * TERRAIN_LAYER0_PARALLAX_DEPTH;
    if (weights.y > 0.001 && HAS_TEXTURE(material, TerrainNormal1))
        depth += weights.y * (1.0 - SAMPLE_TERRAIN_LAYER_HEIGHT(1, position, blending)) * TERRAIN_LAYER1_PARALLAX_DEPTH;
    if (weights.z > 0.001 && HAS_TEXTURE(material, TerrainNormal2))
        depth += weights.z * (1.0 - SAMPLE_TERRAIN_LAYER_HEIGHT(2, position, blending)) * TERRAIN_LAYER2_PARALLAX_DEPTH;
    if (weights.w > 0.001 && HAS_TEXTURE(material, TerrainNormal3))
        depth += weights.w * (1.0 - SAMPLE_TERRAIN_LAYER_HEIGHT(3, position, blending)) * TERRAIN_LAYER3_PARALLAX_DEPTH;

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
    const float detail_fade = lerp(
        TERRAIN_NORMAL_MIN_FADE,
        1.0,
        1.0 - smoothstep(TERRAIN_NORMAL_FADE_START, TERRAIN_NORMAL_FADE_END, view_distance));

    const float macro_tiling_fade = TERRAIN_MACRO_TILING_MAX
        * smoothstep(TERRAIN_MACRO_TILING_FADE_START, TERRAIN_MACRO_TILING_FADE_END, view_distance);

    const float macro_noise = TerrainFbm(P.xz * TERRAIN_MACRO_NOISE_SCALE);
    const float far_noise = TerrainFbm(P.xz * TERRAIN_FAR_NOISE_SCALE + 117.3);
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
    
    const float4 has_layer_texture = float4(
        float(HAS_TEXTURE(material, TerrainLayer0)),
        float(HAS_TEXTURE(material, TerrainLayer1)),
        float(HAS_TEXTURE(material, TerrainLayer2)),
        float(HAS_TEXTURE(material, TerrainLayer3)));

    // layers with no texture hand their weight to the base layer, otherwise it drops to flat material albedo
    const float missing_layer_weight = dot(weights, 1.0 - has_layer_texture);

    weights *= has_layer_texture;
    weights.x += missing_layer_weight * has_layer_texture.x;

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
        layer_detail_heights.x = SAMPLE_TERRAIN_LAYER_HEIGHT(0, detail_position, blending);
    if (weights.y > 0.001 && HAS_TEXTURE(material, TerrainNormal1))
        layer_detail_heights.y = SAMPLE_TERRAIN_LAYER_HEIGHT(1, detail_position, blending);
    if (weights.z > 0.001 && HAS_TEXTURE(material, TerrainNormal2))
        layer_detail_heights.z = SAMPLE_TERRAIN_LAYER_HEIGHT(2, detail_position, blending);
    if (weights.w > 0.001 && HAS_TEXTURE(material, TerrainNormal3))
        layer_detail_heights.w = SAMPLE_TERRAIN_LAYER_HEIGHT(3, detail_position, blending);

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

    float packed_ao = 1.0;

    // the layer under the most weight carries the far tiling; at range one layer covers almost every pixel
    const uint dominant_layer = weights.x >= weights.y
        ? (weights.x >= weights.z ? (weights.x >= weights.w ? 0u : 3u) : (weights.z >= weights.w ? 2u : 3u))
        : (weights.y >= weights.z ? (weights.y >= weights.w ? 1u : 3u) : (weights.z >= weights.w ? 2u : 3u));

    float3 blended_normal = float3(0.0, 0.0, 0.0);
    float total_normal_weight = 0.0;

    if (any_layers)
    {
        albedo = float3(0.0, 0.0, 0.0);
        roughness = 0.0;
        packed_ao = 0.0;

        TerrainLayerAccumulator accumulator;
        accumulator.albedo = float3(0.0, 0.0, 0.0);
        accumulator.roughness = 0.0;
        accumulator.ao = 0.0;
        accumulator.normal = float3(0.0, 0.0, 0.0);
        accumulator.normal_weight = 0.0;

        TerrainLayerContext context;
        context.geometric_normal = N;
        context.normal_strength = TERRAIN_NORMAL_STRENGTH * detail_fade;
        context.detail_position = detail_position;
        context.position_ddx = position_ddx;
        context.position_ddy = position_ddy;
        context.blending = blending;
        context.normal_blending = normal_blending;
        context.antitile_mask = antitile_mask;

        ACCUMULATE_TERRAIN_LAYER(0, weights.x, dominant_layer, macro_tiling_fade, context, accumulator);
        ACCUMULATE_TERRAIN_LAYER(1, weights.y, dominant_layer, macro_tiling_fade, context, accumulator);
        ACCUMULATE_TERRAIN_LAYER(2, weights.z, dominant_layer, macro_tiling_fade, context, accumulator);
        ACCUMULATE_TERRAIN_LAYER(3, weights.w, dominant_layer, macro_tiling_fade, context, accumulator);

        albedo = accumulator.albedo;
        roughness = accumulator.roughness;
        packed_ao = accumulator.ao;
        blended_normal = accumulator.normal;
        total_normal_weight = accumulator.normal_weight;

        // each layer normal already carries the surface orientation, so this replaces N instead of perturbing it
        if (total_normal_weight > 0.001)
        {
            N = normalize(blended_normal);
        }
    }

    // two octaves of brightness plus a hue swing, so distant slopes keep varying once the tiling has mipped away.
    // Summed into one gain rather than multiplied, so the octaves can't compound on top of each other.
    const float macro_variation = (macro_noise - 0.5) * (TERRAIN_MACRO_STRENGTH * 2.0)
        + (far_noise - 0.5) * (TERRAIN_FAR_STRENGTH * 2.0);

    albedo *= max(1.0 + macro_variation, 0.0);
    albedo *= lerp(TERRAIN_MACRO_HUE_COOL, TERRAIN_MACRO_HUE_WARM, saturate(far_noise));

    const float hollow = saturate(concavity);
    const float ridge = saturate(-concavity);

    const float albedo_luminance = dot(albedo, float3(0.2126, 0.7152, 0.0722));

    albedo = lerp(albedo, float3(albedo_luminance, albedo_luminance, albedo_luminance), hollow * TERRAIN_HOLLOW_DESATURATE);
    albedo *= (1.0 - hollow * TERRAIN_HOLLOW_DARKEN) * (1.0 + ridge * TERRAIN_RIDGE_LIGHTEN);

    // scaled rather than clamped per channel, so the ceiling doesn't drag bright materials toward grey
    const float peak_albedo = max(albedo.r, max(albedo.g, albedo.b));
    albedo *= min(1.0, TERRAIN_MAX_ALBEDO / max(peak_albedo, 0.0001));

    roughness = saturate(roughness * (1.0 + (macro_noise - 0.5) * (TERRAIN_ROUGHNESS_NOISE_STRENGTH * 2.0)));

    // erosion hollows and steep faces sit in their own shadow; this only attenuates indirect light downstream
    float material_ao = lerp(1.0, packed_ao, TERRAIN_PACKED_AO_STRENGTH);
    material_ao *= 1.0 - TERRAIN_AO_SLOPE_STRENGTH * smoothstep(0.0, 0.6, slope);
    material_ao *= 1.0 - hollow * TERRAIN_AO_HOLLOW_STRENGTH;
    material_ao = max(material_ao, TERRAIN_AO_MIN);

    output.gbuffer_albedo = float4(albedo, material_ao);

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
