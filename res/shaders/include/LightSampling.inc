#ifndef HYP_LIGHT_SAMPLING
#define HYP_LIGHT_SAMPLING

#include "shared.inc"
#include "brdf.inc"
#include "scene.inc"
#include "material.inc"

static const float lut_size = 64.0;
static const float lut_scale = (lut_size - 1.0) / lut_size;
static const float lut_bias = 0.5 / lut_size;

// References:
// https://blog.selfshadow.com/publications/s2016-advances/s2016_ltc_rnd.pdf
// https://learnopengl.com/code_viewer_gh.php?code=src/8.guest/2022/7.area_lights/2.multiple_area_lights/7.multi_area_light.fs

float IntegrateEdge(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return theta_sintheta;
}

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    return cross(v1, v2) * IntegrateEdge(v1, v2);
}

bool RayPlaneIntersect(in Ray ray, vec4 plane, out float t)
{
    t = -dot(plane, vec4(ray.origin, 1.0)) / dot(plane.xyz, ray.direction);

    return t > 0.0;
}

#ifdef LIGHT_TYPE_AREA_RECT
vec4 SampleRectLightTexture(in Light light, in vec3 pts[4])
{
    if (light.material_index == ~0u || !HAS_TEXTURE(lightMaterial, AlbedoMap))
    {
        return vec4(1.0);
    }

    vec3 v1 = pts[1] - pts[0];
    vec3 v2 = pts[3] - pts[0];
    vec3 ortho = cross(v1, v2);

    float area_sqr = dot(ortho, ortho);
    float dist_area = dot(ortho, pts[0]);

    vec3 P = dist_area * ortho / area_sqr - pts[0];

    float dp_v1_v2 = dot(v1, v2);
    float inv_dp_v1_v1 = 1.0 / dot(v1, v1);
    vec3 v1_perp = v2 - v1 * dp_v1_v2 * inv_dp_v1_v1;

    vec2 uv;
    uv.y = dot(P, v1_perp) / dot(v1_perp, v1_perp);
    uv.x = dot(v1, P) * inv_dp_v1_v1 - dp_v1_v2 * inv_dp_v1_v1 * uv.y;

    float dist = abs(dist_area) / pow(area_sqr, 0.75);

    /// \todo Pre-filter area light texs
    float lod = log(2048.0 * dist) / log(3.0);

    float lod_a = floor(lod);
    float lod_b = ceil(lod);
    float t = lod - lod_a;

    vec4 tex_a = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_LINEAR, GET_TEXTURE(lightMaterial, AlbedoMap), uv, lod_a);
    vec4 tex_b = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_LINEAR, GET_TEXTURE(lightMaterial, AlbedoMap), uv, lod_b);

    return mix(tex_a, tex_b, t);
}

vec4 CalculateAreaLightRadiance(in Light light, in mat3 Minv, in vec3 pts[4], in vec3 P, in vec3 N, in vec3 V)
{
    // construct an orthonormal basis around N
    vec3 T1 = normalize(V - N * dot(V, N));
    vec3 T2 = cross(N, T1);
    mat3 tbn = transpose(mat3(T1, T2, N));

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * tbn;

    // polygon (allocate 4 vertices for clipping)
    vec3 L[4];

    // transform polygon from LTC back to origin Do (cosine weighted)
    L[0] = Minv * (pts[0] - P);
    L[1] = Minv * (pts[1] - P);
    L[2] = Minv * (pts[2] - P);
    L[3] = Minv * (pts[3] - P);

    vec4 sampled_texture = SampleRectLightTexture(light, L);

    for (int i = 0; i < 4; i++) {
        L[i] = normalize(L[i]);
    }

    // use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = pts[0] - P;
    vec3 light_normal = cross(pts[1] - pts[0], pts[3] - pts[0]);
    bool behind = (dot(dir, light_normal) < 0.0);

    vec3 vsum = vec3(0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    // form factor of the polygon in direction vsum
    float len = length(vsum);

    float z = vsum.z / len;
    
    if (behind) {
        z = -z;
    }

    vec2 uv = vec2(z * 0.5 + 0.5, len); // range [0, 1]
    uv.y = 1.0 - uv.y;
    uv = uv * lut_scale + lut_bias;

    // Fetch the form factor for horizon clipping
    float scale = SAMPLE_TEXTURE_2D(ltc_sampler, ltc_brdf_texture, uv).w;

    float sum = len * scale;
    
    if (!behind) {
        sum = 0.0;
    }

    // fix negative values
    sum = max(sum, 0.0);

    return vec4(sum) * sampled_texture;
}

#endif

#endif