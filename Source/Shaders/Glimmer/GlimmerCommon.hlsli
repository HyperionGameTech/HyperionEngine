#ifndef GLIMMER_COMMON_HLSLI
#define GLIMMER_COMMON_HLSLI

#define GLIMMER_GROUND_LEVELS 4
#define GLIMMER_GROUND_RESOLUTION 256
#define GLIMMER_NO_GROUND_HEIGHT -60000.0

#define GLIMMER_GROUND_COVER_LAYERS 4

struct GlimmerGroundLevel
{
    int4 validRect; // absolute texels, xy = min, zw = max (exclusive)
    float4 params;  // x = texel size, y = 1 / texel size
};

struct GlimmerGroundParams
{
    GlimmerGroundLevel levels[GLIMMER_GROUND_LEVELS];
};

struct GlimmerTerrainPatch
{
    float4 worldToObject0;
    float4 worldToObject2;
    float4 boundsXZ; // xy = world xz min, zw = max
    uint4 data;      // x = material index
};

uint2 GlimmerWrapGroundTexel(int2 texel)
{
    return uint2(texel & (GLIMMER_GROUND_RESOLUTION - 1));
}

float3 GlimmerRotateByQuaternion(float4 q, float3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

/// @TODO Unify with other octahedral stuff?
float2 GlimmerOctahedralEncode(float3 direction)
{
    const float3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
    const float2 e = n.y >= 0.0 ? n.xz : (1.0 - abs(n.zx)) * select(n.xz >= 0.0, 1.0, -1.0);

    return e * 0.5 + 0.5;
}

int2 GlimmerOctahedralWrapTexel(int2 texel, int resolution)
{
    if (texel.x < 0 || texel.x >= resolution)
    {
        texel.x = clamp(texel.x, 0, resolution - 1);
        texel.y = resolution - 1 - texel.y;
    }

    if (texel.y < 0 || texel.y >= resolution)
    {
        texel.y = clamp(texel.y, 0, resolution - 1);
        texel.x = resolution - 1 - texel.x;
    }

    return texel;
}

/// @TODO Unify with other octahedral stuff?
float3 GlimmerOctahedralDecode(float2 uv)
{
    const float2 e = uv * 2.0 - 1.0;

    float3 n = float3(e.x, 1.0 - abs(e.x) - abs(e.y), e.y);

    if (n.y < 0.0)
    {
        n.xz = (1.0 - abs(n.zx)) * select(n.xz >= 0.0, 1.0, -1.0);
    }

    return normalize(n);
}

float3 GlimmerSphericalFibonacci(uint index, uint count)
{
    const float goldenRatio = 1.6180339887;

    const float phi = 2.0 * 3.14159265 * frac(float(index) * (goldenRatio - 1.0));
    const float cosTheta = 1.0 - (2.0 * float(index) + 1.0) / float(count);
    const float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));

    return float3(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta);
}

#define GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED 0x1u
#define GLIMMER_INSTANCE_FLAG_MIRRORED 0x2u
#define GLIMMER_INSTANCE_FLAG_ALPHA_TESTED 0x4u
#define GLIMMER_INSTANCE_FLAG_FOLIAGE 0x8u

struct GlimmerSpanInstance
{
    float4 objectToWorld0;
    float4 objectToWorld1;
    float4 objectToWorld2;
    uint4 data; // x = BLAS triangle base, y = triangle count, z = material index, w = flags
};

#define GLIMMER_SPAN_CHUNK_TRIANGLES 256

struct GlimmerSpanChunk
{
    float4 boundsMin; // xyz = the instance's world bounds, w = span instance (as uint)
    float4 boundsMax; // w = first triangle of the chunk, local to the instance (as uint)
};

struct GlimmerSpanLevel
{
    int4 window;   // xy = absolute texel the spans were built from, z = 1 when built
    float4 params; // x = texel size, y = 1 / texel size
};

struct GlimmerSpanParams
{
    GlimmerSpanLevel levels[GLIMMER_GROUND_LEVELS];
};

#define GLIMMER_SPAN_SOLID_MIN 0
#define GLIMMER_SPAN_SOLID_MAX 1
#define GLIMMER_SPAN_CANOPY_MIN 2
#define GLIMMER_SPAN_CANOPY_MAX 3
#define GLIMMER_SPAN_LEAF_AREA 4      // leaf area index
#define GLIMMER_SPAN_CANOPY_ALBEDO 5 // 3 values
#define GLIMMER_SPAN_SOLID_ALBEDO 8  // 3 values
#define GLIMMER_SPAN_SOLID_AREA 11     // surface area / texel area, how filled the texel is
#define GLIMMER_SPAN_VALUES_PER_TEXEL 12

#define GLIMMER_SPAN_AREA_SCALE 256.0

uint GlimmerSpanTexelIndex(uint level, int2 texel)
{
    const uint2 wrapped = GlimmerWrapGroundTexel(texel);

    return ((level * GLIMMER_GROUND_RESOLUTION + wrapped.y) * GLIMMER_GROUND_RESOLUTION + wrapped.x) * GLIMMER_SPAN_VALUES_PER_TEXEL;
}

#define GLIMMER_MASK_EMPTY_MIN 0xFFFFFFFFu
#define GLIMMER_MASK_EMPTY_MAX 0u

uint GlimmerOrderedUintFromFloat(float value)
{
    const uint bits = asuint(value);

    return (bits & 0x80000000u) != 0u ? ~bits : (bits | 0x80000000u);
}

float GlimmerFloatFromOrderedUint(uint value)
{
    return asfloat((value & 0x80000000u) != 0u ? (value & 0x7FFFFFFFu) : ~value);
}

float4 GlimmerBlendFarField(float4 nearField, float4 farField)
{
    const float farWeight = farField.a * (1.0 - nearField.a);
    const float weight = nearField.a + farWeight;

    // @TODO no branch
    if (weight <= 1e-4)
    {
        return (float4)0.0;
    }

    return float4((nearField.rgb * nearField.a + farField.rgb * farWeight) / weight, weight);
}

#endif
