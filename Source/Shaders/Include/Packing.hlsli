#ifndef HYP_PACKING
#define HYP_PACKING

// #define HYP_PACK_NORMALS 1
#define HYP_NORMAL_PACKING_2
// #define HYP_NORMAL_PACKING_1
// #define HYP_NORMAL_PACKING_2

float2 PackNormalVec2(float3 n)
{
#ifdef HYP_NORMAL_PACKING_0
    float scale = 1.7777;
    float2 enc = n.xy / (n.z + 1.0);
    enc /= scale;
    enc = enc * 0.5 + 0.5;
    return enc;
#elif defined(HYP_NORMAL_PACKING_1)
    float p = sqrt(n.z * 8 + 8);
    return n.xy / p + 0.5;
#elif defined(HYP_NORMAL_PACKING_2)
    return (float2(atan(n.x, n.y) / 3.141592654, n.z) + 1.0) * 0.5;
#else
    #error No packing mode defined
#endif
}

float3 UnpackNormalVec2(float2 enc)
{
#ifdef HYP_NORMAL_PACKING_0
    float scale = 1.7777;
    float3 nn =
        float3(enc.xy, 0.0) * float3(2 * scale, 2 * scale, 0) + float3(-scale, -scale, 1);
    float g = 2.0 / dot(nn.xyz, nn.xyz);
    float3 n;
    n.xy = g * nn.xy;
    n.z = g - 1.0;
    return n;
#elif defined(HYP_NORMAL_PACKING_1)
    float2 fenc = enc.xy * 4 - 2;
    float f = dot(fenc, fenc);
    float g = sqrt(1 - f / 4);
    float3 n;
    n.xy = fenc * g;
    n.z = 1 - f / 2;
    return n;
#elif defined(HYP_NORMAL_PACKING_2)
    float2 ang = enc * 2 - 1;
    float2 scth = float2(sin(ang.x * 3.141592654), cos(ang.x * 3.141592654));
    float2 scphi = float2(sqrt(1.0 - ang.y * ang.y), ang.y);
    return float3(scth.y * scphi.x, scth.x * scphi.x, scphi.y);
#else
    #error No packing mode defined
#endif
}

float3 SignedOctEncode(float3 n)
{
    float3 result;

    n /= (abs(n.x) + abs(n.y) + abs(n.z));

    result.y = n.y *  0.5  + 0.5;
    result.x = n.x *  0.5  + result.y;
    result.y = n.x * -0.5  + result.y;

    result.z = clamp(n.z * HYP_FLOAT_MAX, 0.0, 1.0);

    return result;
}

float3 SignedOctDecode(float3 n)
{
    float3 result;

    result.x = (n.x - n.y);
    result.y = (n.x + n.y) - 1.0;
    result.z = n.z * 2.0 - 1.0;
    result.z = result.z * (1.0 - abs(result.x) - abs(result.y));
    
    result = normalize(result);

    return result;
}

float4 EncodeNormal(float3 n)
{
    return float4(SignedOctEncode(n), 0.0);
}

float3 DecodeNormal(float4 enc)
{
    return SignedOctDecode(enc.xyz);
}

static const float4 depthBitShift = float4(256.0 * 256.0 * 256.0, 256.0 * 256.0, 256.0, 1.0);
static const float4 depthBitShiftInv = float4(1.0, 1.0, 1.0, 1.0) / depthBitShift;
static const float4 depthBitMask = float4(0.0, 1.0 / 256.0, 1.0 / 256.0, 1.0 / 256.0);

float4 PackDepth(in float depth)
{
    float4 res = frac(depth.xxxx * depthBitShift);
    res -= res.xxyz * depthBitMask;
    return res;
}

float UnpackDepth(in float4 packedDepth)
{
    return dot(packedDepth, depthBitShiftInv);
}

#define HYP_QUANTIZE(v, bits) uint(round(clamp((v), 0.0, 1.0) * ((1 << (bits)) - 1)))
#define HYP_UNQUANTIZE(x, bits) float(x) / float((1 << (bits)) - 1)

#endif
