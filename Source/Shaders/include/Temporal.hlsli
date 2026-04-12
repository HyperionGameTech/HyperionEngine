#ifndef HYP_TEMPORAL
#define HYP_TEMPORAL

#include "./Shared.hlsli"

static const float spatial_offsets[] = { 0.0, 0.5, 0.25, 0.75 };
static const float temporal_rotations[] = { 60, 300, 180, 240, 120, 0 };

#define HYP_TAA_NEIGHBORS_3x3 9
#define HYP_TAA_NEIGHBORS_2x2 5

#ifndef FEEDBACK
#define FEEDBACK 0.8
#endif

#ifdef TEMPORAL_BLENDING_GAMMA_CORRECTION
#define ADJUST_COLOR_GAMMA_IN(col) \
    (vec4(pow(col.rgb, vec3(2.2, 2.2, 2.2)), col.a))

#define ADJUST_COLOR_GAMMA_OUT(col) \
    (vec4(pow(col.rgb, vec3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)), col.a))
#elif defined(TEMPORAL_BLENDING_REVERSE_TONEMAP)
#define ADJUST_COLOR_GAMMA_IN(col) \
    (vec4(ReverseTonemapReinhardSimple(col.rgb), col.a))

#define ADJUST_COLOR_GAMMA_OUT(col) \
    (vec4(ReverseTonemapReinhardSimple(col.rgb), col.a))
#else
#define ADJUST_COLOR_GAMMA_IN(col) \
    (col)

#define ADJUST_COLOR_GAMMA_OUT(col) \
    (col)
#endif

#ifdef ADJUST_COLOR_HDR
#define ADJUST_COLOR_IN(col) \
    ((AdjustColorIn(col)))

#define ADJUST_COLOR_OUT(col) \
    ((AdjustColorOut(col)))
#else
#define ADJUST_COLOR_IN(col) \
    (col)

#define ADJUST_COLOR_OUT(col) \
    (col)
#endif

#define ADJUST_COLOR_YCoCg_IN(col) \
    (RGBToYCoCg(col))
#define ADJUST_COLOR_YCoCg_OUT(col) \
    (YCoCgToRGB(col))

static const vec2 neighbor_uv_offsets_2x2[HYP_TAA_NEIGHBORS_2x2] = {
    vec2(-1.0, 0.0),
    vec2(0.0, -1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0)
};

static const vec2 neighbor_uv_offsets_3x3[HYP_TAA_NEIGHBORS_3x3] = {
    vec2(-1.0, -1.0),
    vec2(0.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 0.0),
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(-1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
};

float GetSpatialOffset(uint frame_counter)
{
    return spatial_offsets[(frame_counter / 6) % 4];
}

float GetTemporalRotation(uint frame_counter)
{
    return temporal_rotations[frame_counter % 6];
}

vec4 AdjustColorIn(in vec4 color)
{
    // watch out for NaN
    return vec4(log(color.rgb), color.a);
}

vec4 AdjustColorOut(in vec4 color)
{
    // watch out for NaN
    return vec4(exp(color.rgb), color.a);
}

void GetPixelNeighbors_3x3(in texture2D tex, in vec2 uv, in vec2 texel_size, out vec4 neighbors[9])
{
    vec2 offset_uv;

    for (uint i = 0; i < 9; i++)
    {
        offset_uv = uv + (neighbor_uv_offsets_3x3[i] * texel_size);

        vec4 neighbor_color = AdjustColorIn(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, tex, offset_uv, 0.0));

        neighbors[i] = neighbor_color;
    }
}

void GetPixelNeighborsMinMax_3x3(in texture2D tex, in vec2 uv, in vec2 texel_size, out vec4 min_value, out vec4 max_value)
{
    vec4 _min_value = vec4(1000000.0, 1000000.0, 1000000.0, 1000000.0);
    vec4 _max_value = vec4(-1000000.0, -1000000.0, -1000000.0, -1000000.0);

    vec2 offset_uv;

    for (uint i = 0; i < 9; i++)
    {
        offset_uv = uv + (neighbor_uv_offsets_3x3[i] * texel_size);

        vec4 neighbor_color = AdjustColorIn(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, tex, offset_uv, 0.0));

        _min_value = min(_min_value, neighbor_color);
        _max_value = max(_max_value, neighbor_color);
    }

    min_value = _min_value;
    max_value = _max_value;
}

void GetPixelTexelNeighborsMinMax_3x3(in texture2D tex, in ivec2 coord, in ivec2 dimensions, out vec4 min_value, out vec4 max_value)
{
    vec4 _min_value = vec4(1000000.0, 1000000.0, 1000000.0, 1000000.0);
    vec4 _max_value = vec4(-1000000.0, -1000000.0, -1000000.0, -1000000.0);

    ivec2 offset_coord;

    for (uint i = 0; i < 9; i++)
    {
        offset_coord = coord + ivec2(neighbor_uv_offsets_3x3[i]);
        offset_coord = clamp(offset_coord, ivec2(0, 0), dimensions - 1);

        vec4 neighbor_color = AdjustColorIn(TEXEL_FETCH_2D_LOD(sampler_nearest, tex, offset_coord, 0));

        _min_value = min(_min_value, neighbor_color);
        _max_value = max(_max_value, neighbor_color);
    }

    min_value = _min_value;
    max_value = _max_value;
}

// vec4 MinColors_3x3(in vec4 colors[9])
// {
//     vec4 result = colors[0];

//     for (uint i = 1; i < 9; i++) {
//         result = min(result, colors[i]);
//     }

//     return result;
// }

// vec4 MaxColors_3x3(in vec4 colors[9])
// {
//     vec4 result = colors[0];

//     for (uint i = 1; i < 9; i++) {
//         result = max(result, colors[i]);
//     }

//     return result;
// }

vec4 ClipAABB(vec4 aabb_min, vec4 aabb_max, vec4 p, vec4 q)
{
    vec4 r = q - p;
    vec4 rmax = aabb_max - p;
    vec4 rmin = aabb_min - p;

    const float eps = HYP_FMATH_EPSILON;

    if (r.x > rmax.x + eps)
        r *= (rmax.x / r.x);
    if (r.y > rmax.y + eps)
        r *= (rmax.y / r.y);
    if (r.z > rmax.z + eps)
        r *= (rmax.z / r.z);
    if (r.w > rmax.w + eps)
        r *= (rmax.w / r.w);

    if (r.x < rmin.x - eps)
        r *= (rmin.x / r.x);
    if (r.y < rmin.y - eps)
        r *= (rmin.y / r.y);
    if (r.z < rmin.z - eps)
        r *= (rmin.z / r.z);
    if (r.w < rmin.w - eps)
        r *= (rmin.w / r.w);

    return p + r;
}

#define VARIANCE_INTERSECTION_MAX_T 10000.0

#if 1
vec4 ClipToAABB(in vec4 color, in vec4 previous_color, in vec4 avg, in vec4 half_size)
{
    // if (all(lessThanEqual(abs(previous_color - avg), half_size))) {
    //     return previous_color;
    // }

    vec4 dir = (color - previous_color);
    vec4 near = avg - sign(dir) * half_size;
    vec4 tAll = (near - previous_color) / dir;
    float t = VARIANCE_INTERSECTION_MAX_T;

    // clip unexpected T values
    // const vec4 possibleT = lerp(vec4(VARIANCE_INTERSECTION_MAX_T + 1.0), tAll, greaterThanEqual(tAll, vec4(0.0, 0.0, 0.0, 0.0)));
    // const float t = min(VARIANCE_INTERSECTION_MAX_T, min(possibleT.x, min(possibleT.y, min(possibleT.z, possibleT.w))));

    for (int i = 0; i < 4; i++)
    {
        if (tAll[i] >= 0.0 && tAll[i] < t)
        {
            t = tAll[i];
        }
    }

#ifdef LANG_GLSL
    return lerp(previous_color, previous_color + dir * t, bvec4(t < VARIANCE_INTERSECTION_MAX_T));
#else
    return lerp(previous_color, previous_color + dir * t, HYP_FAST_LESS(t, VARIANCE_INTERSECTION_MAX_T));
#endif
}

#elif 0
vec4 ClipToAABB(vec4 inCurrentColour, vec4 inHistoryColour, vec4 inBBCentre, vec4 inBBExtents)
{
    const vec4 direction = inCurrentColour - inHistoryColour;

    // calculate intersection for the closest slabs from the center of the AABB in HistoryColour direction
    const vec4 intersection = ((inBBCentre - sign(direction) * inBBExtents) - inHistoryColour) / direction;

    // clip unexpected T values
    const vec4 possibleT = lerp(vec4(VARIANCE_INTERSECTION_MAX_T + 1.0), intersection, greaterThanEqual(intersection, vec4(0.0, 0.0, 0.0, 0.0)));
    const vec4 t = vec4(min(VARIANCE_INTERSECTION_MAX_T, min(possibleT.x, min(possibleT.y, min(possibleT.z, possibleT.w)))));

    // final history colour
    return lerp(inHistoryColour, inHistoryColour + direction * t, lessThan(t, vec4(VARIANCE_INTERSECTION_MAX_T)));
}
#else
vec4 ClipToAABB(in vec4 color, in vec4 previous_color, in vec4 avg, in vec4 half_size)
{
    if (all(lessThanEqual(abs(previous_color - avg), half_size)))
    {
        return previous_color;
    }

    vec4 dir = (color - previous_color);
    vec4 near = avg - sign(dir) * half_size;
    vec4 tAll = (near - previous_color) / dir;
    float t = 1e20;
    for (int i = 0; i < 4; i++)
    {
        if (tAll[i] >= 0.0 && tAll[i] < t)
        {
            t = tAll[i];
        }
    }

    if (t >= 1e20)
    {
        return previous_color;
    }
    return previous_color + dir * t;
}
#endif

vec3 ClosestFragment_3x3(in texture2D depth_texture, vec2 uv, vec2 texel_size)
{
    vec2 dd = abs(texel_size.xy);
    vec2 du = vec2(dd.x, 0.0);
    vec2 dv = vec2(0.0, dd.y);

    vec3 dtl = vec3(-1, -1, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv - dv - du).x);
    vec3 dtc = vec3(0, -1, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv - dv).x);
    vec3 dtr = vec3(1, -1, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv - dv + du).x);

    vec3 dml = vec3(-1, 0, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv - du).x);
    vec3 dmc = vec3(0, 0, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv).x);
    vec3 dmr = vec3(1, 0, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv + du).x);

    vec3 dbl = vec3(-1, 1, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv + dv - du).x);
    vec3 dbc = vec3(0, 1, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv + dv).x);
    vec3 dbr = vec3(1, 1, SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, uv + dv + du).x);

    vec3 dmin = dtl;
    if (dmin.z > dtc.z)
        dmin = dtc;
    if (dmin.z > dtr.z)
        dmin = dtr;

    if (dmin.z > dml.z)
        dmin = dml;
    if (dmin.z > dmc.z)
        dmin = dmc;
    if (dmin.z > dmr.z)
        dmin = dmr;

    if (dmin.z > dbl.z)
        dmin = dbl;
    if (dmin.z > dbc.z)
        dmin = dbc;
    if (dmin.z > dbr.z)
        dmin = dbr;

    return vec3(uv + dd.xy * dmin.xy, dmin.z);
}

vec3 ClosestFragment(in texture2D depth_texture, vec2 uv, vec2 texel_size)
{
    vec2 closest_uv = uv;
    float closest_depth = 1000.0;

    for (uint i = 0; i < HYP_TAA_NEIGHBORS_3x3; i++)
    {
        vec2 offset_uv = uv + (neighbor_uv_offsets_3x3[i] * texel_size);
        float neighbor_depth = SAMPLE_TEXTURE_2D(sampler_nearest, depth_texture, offset_uv).r;

        if (neighbor_depth < closest_depth)
        {
            closest_depth = neighbor_depth;
            closest_uv = offset_uv;
        }
    }

    return vec3(closest_uv, closest_depth);
}

vec4 MinColors_2x2(in vec4 colors[HYP_TAA_NEIGHBORS_2x2])
{
    vec4 result = colors[0];

    for (uint i = 1; i < HYP_TAA_NEIGHBORS_2x2; i++)
    {
        result = min(result, colors[i]);
    }

    return result;
}

vec4 MaxColors_2x2(in vec4 colors[HYP_TAA_NEIGHBORS_2x2])
{
    vec4 result = colors[0];

    for (uint i = 1; i < HYP_TAA_NEIGHBORS_2x2; i++)
    {
        result = max(result, colors[i]);
    }

    return result;
}

vec4 MinColors_3x3(in vec4 colors[HYP_TAA_NEIGHBORS_3x3])
{
    vec4 result = colors[0];

    for (uint i = 1; i < HYP_TAA_NEIGHBORS_3x3; i++)
    {
        result = min(result, colors[i]);
    }

    return result;
}

vec4 MaxColors_3x3(in vec4 colors[HYP_TAA_NEIGHBORS_3x3])
{
    vec4 result = colors[0];

    for (uint i = 1; i < HYP_TAA_NEIGHBORS_3x3; i++)
    {
        result = max(result, colors[i]);
    }

    return result;
}

vec4 ColorClamping(vec4 color_min, vec4 color_max, vec4 current_color, vec4 previous_color)
{
    vec3 p_clip = (color_max.rgb + color_min.rgb) * 0.5;
    vec3 e_clip = (color_max.rgb - color_min.rgb) * 0.5;

    vec4 v_clip = previous_color - vec4(p_clip, current_color.a);

    vec3 v_unit = v_clip.rgb / e_clip;

    vec3 a_unit = abs(v_unit);

    float max_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (max_unit > 1.0)
    {
        return vec4(p_clip, current_color.a) + v_clip / max_unit;
    }
    else
    {
        return previous_color;
    }
}

vec4 PixelHistory(in vec4 current_color, in vec4 previous_color, in vec4 colors[HYP_TAA_NEIGHBORS_3x3])
{
    vec4 color_min = MinColors_3x3(colors);
    vec4 color_max = MaxColors_3x3(colors);

    // return ColorClamping(color_min, color_max, current_color, previous_color);
    return clamp(previous_color, color_min, color_max);
}

vec4 TemporalLuminanceResolve(vec4 color, vec4 color_clipped)
{
    const float lum0 = Luminance(color.rgb);
    const float lum1 = Luminance(color_clipped.rgb);

    float unbiased_diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = HYP_FMATH_SQR(unbiased_weight);
    float feedback = saturate(lerp(0.88, 0.98, unbiased_weight_sqr));

    return lerp(color, color_clipped, feedback);
}

vec4 TemporalLuminanceResolveYCoCg(vec4 color, vec4 color_clipped)
{
    const float lum0 = color.r;
    const float lum1 = color_clipped.r;

    float unbiased_diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = HYP_FMATH_SQR(unbiased_weight);
    float feedback = saturate(lerp(0.88, 0.98, unbiased_weight_sqr));

    return lerp(color, color_clipped, feedback);
}

vec4 TemporalResolve(in texture2D color_texture, in texture2D previous_color_texture, vec2 uv, vec2 velocity, vec2 texel_size, float view_space_depth)
{
    const float _SubpixelThreshold = 0.5;
    const float _GatherBase = 0.5;
    const float _GatherSubpixelMotion = 0.1666;

    const vec2 texel_vel = velocity / max(vec2(HYP_FMATH_EPSILON, HYP_FMATH_EPSILON), texel_size);
    const float texel_vel_mag = length(texel_vel) * view_space_depth;
    const float subpixel_motion = saturate(_SubpixelThreshold / max(HYP_FMATH_EPSILON, texel_vel_mag));
    const float min_max_support = _GatherBase + _GatherSubpixelMotion * subpixel_motion;

    vec4 current_colors_3x3[HYP_TAA_NEIGHBORS_3x3];
    vec4 previous_colors_3x3[HYP_TAA_NEIGHBORS_3x3];

    vec2 offset_uv;

    for (uint i = 0; i < HYP_TAA_NEIGHBORS_3x3; i++)
    {
        offset_uv = uv + (neighbor_uv_offsets_3x3[i] * texel_size);

        current_colors_3x3[i] = ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, color_texture, offset_uv, 0.0)));
        previous_colors_3x3[i] = ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, previous_color_texture, offset_uv - velocity, 0.0)));
    }

    vec4 current_color_min_3x3 = MinColors_3x3(current_colors_3x3);
    vec4 previous_color_max_3x3 = MaxColors_3x3(previous_colors_3x3);

    // TODO: just set to 3x3 items at indices 3, 1, 4, 5, 8 ??
    /// even better, just use those indices as the first 5 items in the 3x3 list,
    // and calc them together?
    vec4 current_colors_2x2[HYP_TAA_NEIGHBORS_2x2];
    vec4 previous_colors_2x2[HYP_TAA_NEIGHBORS_2x2];

    for (uint i = 0; i < HYP_TAA_NEIGHBORS_2x2; i++)
    {
        offset_uv = uv + (neighbor_uv_offsets_2x2[i] * texel_size);

        current_colors_2x2[i] = ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, color_texture, offset_uv, 0.0)));
        previous_colors_2x2[i] = ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, previous_color_texture, offset_uv - velocity, 0.0)));
    }

    vec4 current_color_min_2x2 = MinColors_2x2(current_colors_2x2);
    vec4 previous_color_max_2x2 = MaxColors_2x2(previous_colors_2x2);

    vec4 current_color_min = lerp(current_color_min_3x3, current_color_min_2x2, 0.5);
    vec4 previous_color_max = lerp(previous_color_max_3x3, previous_color_max_2x2, 0.5);

    const float feedback = FEEDBACK;
    const float velocity_scale = 8.0;
    const float blend = saturate(feedback - ((length(velocity) - 0.0001) * velocity_scale));

    const vec4 current_color = current_colors_2x2[2];
    const vec4 previous_color = previous_colors_2x2[2];
    const vec4 previous_color_constrained = PixelHistory(current_color, previous_color, current_colors_3x3); // previous_colors_2x2);

    vec4 result = lerp(current_color, previous_color_constrained, blend);
    return ADJUST_COLOR_GAMMA_OUT(TemporalLuminanceResolve(ADJUST_COLOR_OUT(current_color), ADJUST_COLOR_OUT(previous_color_constrained)));
}

void InitTemporalParams(
    in texture2D depth_texture,
    in texture2D velocity_texture,
    in vec2 depth_texture_dimensions,
    in vec2 uv,
    in float camera_near,
    in float camera_far,
    out vec2 velocity,
    out float view_space_depth)
{
    const vec2 depth_texel_size = vec2(1.0, 1.0) / vec2(depth_texture_dimensions);
    const vec3 closest_fragment = ClosestFragment(depth_texture, uv, depth_texel_size);

    velocity = SAMPLE_TEXTURE_2D(sampler_nearest, velocity_texture, closest_fragment.xy).rg;
    view_space_depth = ViewDepth(closest_fragment.z, camera_near, camera_far);
}

vec4 TemporalBlendRounded(in texture2D input_texture, in texture2D prev_input_texture, vec2 uv, vec2 velocity, vec2 texel_size, float view_space_depth)
{
    // Read center and history, apply gamma and HDR/log adjust, then convert to YCoCg
    vec4 color_rgb = ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv));
    vec4 previous_rgb = ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, prev_input_texture, uv - velocity));

    vec4 color_adj = ADJUST_COLOR_IN(color_rgb);
    vec4 previous_adj = ADJUST_COLOR_IN(previous_rgb);

    vec4 color = RGBToYCoCg(color_adj);
    const vec4 previous_color = RGBToYCoCg(previous_adj);

    const float _SubpixelThreshold = 0.5;
    const float _GatherBase = 0.5;
    const float _GatherSubpixelMotion = 0.3333;

    const vec2 texel_vel = velocity / max(vec2(HYP_FMATH_EPSILON, HYP_FMATH_EPSILON), texel_size);
    const float texel_vel_mag = length(texel_vel) * view_space_depth;
    const float subpixel_motion = saturate(_SubpixelThreshold / max(HYP_FMATH_EPSILON, texel_vel_mag));
    const float min_max_support = _GatherBase + _GatherSubpixelMotion * subpixel_motion;

    vec2 du = vec2(texel_size.x, 0.0);
    vec2 dv = vec2(0.0, texel_size.y);

    // Neighbourhood for AABB should use exact texels (nearest) -> gamma -> HDR/log -> YCoCg
    vec4 ctl = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv - dv - du))));
    vec4 ctc = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv - dv))));
    vec4 ctr = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv - dv + du))));
    vec4 cml = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv - du))));
    vec4 cmc = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv))));
    vec4 cmr = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv + du))));
    vec4 cbl = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv + dv - du))));
    vec4 cbc = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv + dv))));
    vec4 cbr = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_nearest, input_texture, uv + dv + du))));

    vec4 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
    vec4 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

    vec4 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

    vec4 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
    vec4 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
    vec4 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
    cmin = 0.5 * (cmin + cmin5);
    cmax = 0.5 * (cmax + cmax5);
    cavg = 0.5 * (cavg + cavg5);

    // color is already in YCoCg (after ADJUST_COLOR_IN), so use its chroma directly
    vec2 chroma_extent = vec2((0.25 * 0.5 * (cmax.r - cmin.r)).xx);
    vec2 chroma_center = color.gb;
    cmin.yz = chroma_center - chroma_extent;
    cmax.yz = chroma_center + chroma_extent;
    cavg.yz = chroma_center;

    vec4 clipped = clamp(cavg, cmin, cmax);
    // ClipAABB expects values in the same (YCoCg + adjusted) space - pass previous_color (already in that space)
    clipped = ClipAABB(cmin, cmax, clipped, previous_color);

    // Resolve in YCoCg, convert back to RGB, then undo HDR/log and gamma-correct
    vec4 resolved_yc = TemporalLuminanceResolveYCoCg(color, clipped);
    vec4 resolved_rgb = YCoCgToRGB(resolved_yc);
    vec4 out_rgb = ADJUST_COLOR_OUT(resolved_rgb);
    return ADJUST_COLOR_GAMMA_OUT(out_rgb);
}

vec4 TemporalBlendVarying(in texture2D input_texture, in texture2D prev_input_texture, vec2 uv, vec2 velocity, vec2 texel_size, float view_space_depth)
{
    // Read and prepare current and previous pixels: gamma -> HDR/log -> YCoCg
    vec4 color_rgb = ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv));
    vec4 previous_rgb = ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, prev_input_texture, uv - velocity));

    vec4 color_adj = ADJUST_COLOR_IN(color_rgb);
    vec4 previous_adj = ADJUST_COLOR_IN(previous_rgb);

    const vec4 color = RGBToYCoCg(color_adj);
    const vec4 previous_color = RGBToYCoCg(previous_adj);

    const float _SubpixelThreshold = 0.5;
    const float _GatherBase = 0.5;
    const float _GatherSubpixelMotion = 0.1667;

    const vec2 texel_vel = velocity / max(vec2(HYP_FMATH_EPSILON, HYP_FMATH_EPSILON), texel_size);
    const float texel_vel_mag = length(texel_vel) * view_space_depth;
    const float subpixel_motion = saturate(_SubpixelThreshold / max(HYP_FMATH_EPSILON, texel_vel_mag));
    const float min_max_support = _GatherBase + _GatherSubpixelMotion * subpixel_motion;

    const vec2 ss_offset01 = min_max_support * vec2(-texel_size.x, texel_size.y);
    const vec2 ss_offset11 = min_max_support * vec2(texel_size.x, texel_size.y);

    // Sample neighborhood with linear filtering (offsets may be fractional). Apply gamma and HDR/log, then convert
    const vec4 c00 = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv - ss_offset11))));
    const vec4 c10 = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv - ss_offset01))));
    const vec4 c01 = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv + ss_offset01))));
    const vec4 c11 = RGBToYCoCg(ADJUST_COLOR_IN(ADJUST_COLOR_GAMMA_IN(SAMPLE_TEXTURE_2D(sampler_linear, input_texture, uv + ss_offset11))));

    vec4 cmin = min(c00, min(c10, min(c01, c11)));
    vec4 cmax = max(c00, max(c10, max(c01, c11)));
    vec4 cavg = (c00 + c10 + c01 + c11) / 4.0;

    vec2 chroma_extent = vec2((0.25 * 0.5 * (cmax.r - cmin.r)).xx);
    vec2 chroma_center = color.gb;
    cmin.yz = chroma_center - chroma_extent;
    cmax.yz = chroma_center + chroma_extent;
    cavg.yz = chroma_center;

    // ClipAABB and TemporalLuminanceResolve operate in YCoCg+adjusted space
    const vec4 clipped = ClipAABB(cmin, cmax, clamp(cavg, cmin, cmax), previous_color);

    vec4 resolved_yc = TemporalLuminanceResolveYCoCg(color, clipped);
    vec4 resolved_rgb = YCoCgToRGB(resolved_yc);
    vec4 out_rgb = ADJUST_COLOR_OUT(resolved_rgb);
    return ADJUST_COLOR_GAMMA_OUT(out_rgb);
}

#endif