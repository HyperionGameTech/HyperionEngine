#ifndef HYP_LIGHT_SAMPLING
#define HYP_LIGHT_SAMPLING

#include "Shared.hlsli"
#include "BRDF.hlsli"
#include "Scene.hlsli"
#include "Material.hlsli"

static const float lut_size = 64.0;
static const float lut_scale = (lut_size - 1.0) / lut_size;
static const float lut_bias = 0.5 / lut_size;

// Linearly Transformed Cosines, adapted from the reference implementation (webgl/shaders/ltc/ltc_quad.fs) at
// https://github.com/selfshadow/ltc_code
// Copyright (c) 2017, Eric Heitz, Jonathan Dupuy, Stephen Hill and David Neubelt. BSD-3-Clause, see THIRD_PARTY_NOTICES.md
// Eric Heitz, Jonathan Dupuy, Stephen Hill and David Neubelt. Real-Time Polygonal-Light Shading with Linearly Transformed Cosines.
// ACM Transactions on Graphics (Proceedings of ACM SIGGRAPH 2016) 35(4), 2016.
// Textured lights: https://blog.selfshadow.com/publications/s2016-advances/s2016_ltc_rnd.pdf

float3 IntegrateEdgeVec(float3 v1, float3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    // rational fit of theta / sin(theta), avoids acos()
    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

bool RayPlaneIntersect(in Ray ray, float4 plane, out float t)
{
    t = -dot(plane, float4(ray.origin, 1.0)) / dot(plane.xyz, ray.direction);

    return t > 0.0;
}

#ifdef LIGHT_TYPE_AREA_RECT
float4 SampleRectLightTexture(in Light light, in float3 pts[4])
{
    if (light.material_index == ~0u || !HAS_TEXTURE(lightMaterial, DiffuseMap))
    {
        return float4(1.0, 1.0, 1.0, 1.0);
    }

    float3 v1 = pts[1] - pts[0];
    float3 v2 = pts[3] - pts[0];
    float3 ortho = cross(v1, v2);

    float area_sqr = dot(ortho, ortho);
    float dist_area = dot(ortho, pts[0]);

    float3 P = dist_area * ortho / area_sqr - pts[0];

    float dp_v1_v2 = dot(v1, v2);
    float inv_dp_v1_v1 = 1.0 / dot(v1, v1);
    float3 v1_perp = v2 - v1 * dp_v1_v2 * inv_dp_v1_v1;

    float2 uv;
    uv.y = dot(P, v1_perp) / dot(v1_perp, v1_perp);
    uv.x = dot(v1, P) * inv_dp_v1_v1 - dp_v1_v2 * inv_dp_v1_v1 * uv.y;

    float dist = abs(dist_area) / pow(area_sqr, 0.75);

    /// \todo Pre-filter area light texs
    float lod = log(2048.0 * dist) / log(3.0);

    float lod_a = floor(lod);
    float lod_b = ceil(lod);
    float t = lod - lod_a;

    float4 tex_a = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_LINEAR, GET_TEXTURE(lightMaterial, DiffuseMap), uv, lod_a);
    float4 tex_b = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_LINEAR, GET_TEXTURE(lightMaterial, DiffuseMap), uv, lod_b);

    return lerp(tex_a, tex_b, t);
}

float4 CalculateAreaLightRadiance(in Light light, in float3x3 Minv, in float3 pts[4], in float3 P, in float3 N, in float3 V)
{
    // construct orthonormal basis around N
    float3 T1 = normalize(V - N * dot(V, N));
    float3 T2 = cross(N, T1);
    float3x3 tbn = transpose(float3x3(T1, T2, N));

    // rotate area light in (T1, T2, N) basis
    Minv = mul(tbn, Minv);

    float3 L[4];
    L[0] = mul(pts[0] - P, Minv);
    L[1] = mul(pts[1] - P, Minv);
    L[2] = mul(pts[2] - P, Minv);
    L[3] = mul(pts[3] - P, Minv);

    float4 sampled_texture = SampleRectLightTexture(light, L);

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        L[i] = normalize(L[i]);
    }

    // clipless approximation: the sphere form factor table in ltc_brdf_texture.w accounts for horizon clipping
    float3 dir = pts[0] - P;
    float3 light_normal = cross(pts[1] - pts[0], pts[3] - pts[0]);
    bool behind = (dot(dir, light_normal) < 0.0);

    float3 vsum = float3(0.0, 0.0, 0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z / len;

    if (behind)
        z = -z;

    float2 uv = float2(z * 0.5 + 0.5, len);
    uv.y = 1.0 - uv.y;
    uv = uv * lut_scale + lut_bias;

    float scale = SAMPLE_TEXTURE_2D(ltc_sampler, ltc_brdf_texture, uv).w;

    float sum = max(select(behind, 0.0, len * scale), 0.0);

    return sampled_texture * sum;
}

#endif

#endif