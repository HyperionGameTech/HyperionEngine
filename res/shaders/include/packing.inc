#ifndef HYP_PACKING
#define HYP_PACKING

// #define HYP_PACK_NORMALS 1
#define HYP_NORMAL_PACKING_2
// #define HYP_NORMAL_PACKING_1
// #define HYP_NORMAL_PACKING_2

vec2 PackNormalVec2(vec3 n)
{
#ifdef HYP_NORMAL_PACKING_0
    float scale = 1.7777;
    vec2 enc = n.xy / (n.z + 1.0);
    enc /= scale;
    enc = enc * 0.5 + 0.5;
    return enc;
#elif defined(HYP_NORMAL_PACKING_1)
    float p = sqrt(n.z * 8 + 8);
    return n.xy / p + 0.5;
#elif defined(HYP_NORMAL_PACKING_2)
    return (vec2(atan(n.x, n.y) / 3.141592654, n.z) + 1.0) * 0.5;
#else
    #error No packing mode defined
#endif
}

vec3 UnpackNormalVec2(vec2 enc)
{
#ifdef HYP_NORMAL_PACKING_0
    float scale = 1.7777;
    vec3 nn =
        vec3(enc.xy, 0.0) * vec3(2 * scale, 2 * scale, 0) + vec3(-scale, -scale, 1);
    float g = 2.0 / dot(nn.xyz, nn.xyz);
    vec3 n;
    n.xy = g * nn.xy;
    n.z = g - 1.0;
    return n;
#elif defined(HYP_NORMAL_PACKING_1)
    vec2 fenc = enc.xy * 4 - 2;
    float f = dot(fenc, fenc);
    float g = sqrt(1 - f / 4);
    vec3 n;
    n.xy = fenc * g;
    n.z = 1 - f / 2;
    return n;
#elif defined(HYP_NORMAL_PACKING_2)
    vec2 ang = enc * 2 - 1;
    vec2 scth = vec2(sin(ang.x * 3.141592654), cos(ang.x * 3.141592654));
    vec2 scphi = vec2(sqrt(1.0 - ang.y * ang.y), ang.y);
    return vec3(scth.y * scphi.x, scth.x * scphi.x, scphi.y);
#else
    #error No packing mode defined
#endif
}

vec3 SignedOctEncode(vec3 n)
{
    vec3 result;

    n /= (abs(n.x) + abs(n.y) + abs(n.z));

    result.y = n.y *  0.5  + 0.5;
    result.x = n.x *  0.5  + result.y;
    result.y = n.x * -0.5  + result.y;

    result.z = clamp(n.z * HYP_FLOAT_MAX, 0.0, 1.0);

    return result;
}

vec3 SignedOctDecode(vec3 n)
{
    vec3 result;

    result.x = (n.x - n.y);
    result.y = (n.x + n.y) - 1.0;
    result.z = n.z * 2.0 - 1.0;
    result.z = result.z * (1.0 - abs(result.x) - abs(result.y));
    
    result = normalize(result);

    return result;
}

vec4 EncodeNormal(vec3 n)
{
    return vec4(SignedOctEncode(n), 0.0);
}

vec3 DecodeNormal(vec4 enc)
{
    return SignedOctDecode(enc.xyz);
}

#ifdef LANG_HLSL
static const float4 depthBitShift = float4(256.0 * 256.0 * 256.0, 256.0 * 256.0, 256.0, 1.0);
static const float4 depthBitShiftInv = float4(1.0, 1.0, 1.0, 1.0) / depthBitShift;
static const float4 depthBitMask = float4(0.0, 1.0 / 256.0, 1.0 / 256.0, 1.0 / 256.0);
#else
const vec4 depthBitShift = vec4(256.0 * 256.0 * 256.0, 256.0 * 256.0, 256.0, 1.0);
const vec4 depthBitShiftInv = vec4(1.0, 1.0, 1.0, 1.0) / depthBitShift;
const vec4 depthBitMask = vec4(0.0, 1.0 / 256.0, 1.0 / 256.0, 1.0 / 256.0);
#endif

vec4 PackDepth(in float depth)
{
    vec4 res = fract(depth.xxxx * depthBitShift);
    res -= res.xxyz * depthBitMask;
    return res;
}

float UnpackDepth(in vec4 packedDepth)
{
    return dot(packedDepth, depthBitShiftInv);
}

#define HYP_QUANTIZE(v, bits) uint(round(clamp((v), 0.0, 1.0) * ((1 << (bits)) - 1)))
#define HYP_UNQUANTIZE(x, bits) float(x) / float((1 << (bits)) - 1)

#endif
