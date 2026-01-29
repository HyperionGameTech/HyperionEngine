#ifndef HYP_OCTAHEDRON
#define HYP_OCTAHEDRON

vec2 EncodeOctahedralCoord(in vec3 v)
{
#define NON_ZERO_SIGN(n) (n >= 0.0 ? 1.0 : -1.0)
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 vec = v.xy * (1.0 / l1norm);

    if (v.z < 0.0) {
        vec = (1.0 - abs(vec.yx)) * vec2(NON_ZERO_SIGN(vec.x), NON_ZERO_SIGN(vec.y));
    }
#undef NON_ZERO_SIGN

    return vec;
}

vec3 DecodeOctahedralCoord(vec2 coord)
{
#define NON_ZERO_SIGN(n) (n >= 0.0 ? 1.0 : -1.0)
    vec3 vec = vec3(coord.x, coord.y, 1.0 - abs(coord.x) - abs(coord.y));
    
    if (vec.z < 0.0) {
        vec.xy = (1.0 - abs(vec.yx)) * vec2(NON_ZERO_SIGN(vec.x), NON_ZERO_SIGN(vec.y));
    }
#undef NON_ZERO_SIGN
    
    return normalize(vec);
}

#endif