//
// Created by emd22 on 11/10/22.
//

#ifndef HYPERION_INTRINSICS_HPP
#define HYPERION_INTRINSICS_HPP

#include <immintrin.h>
#include <util/Defines.hpp>

namespace hyperion::intrinsics {


#if HYP_FEATURES_INTRINSICS && !defined(HYP_ARM)
/* Float 256, 8 single precision floats */
using Float256  = __m256;
/* Float 512, 16 single precision floats */
using Float512  = __m512;
#else

struct Float256 {
    float values[8];
};

struct Float512 {
    float values[16];
};

#endif

using Double256 = __m256d;
using Double512 = __m512d;
using Int256    = __m256i;
using Int512    = __m512i;
using Mask16     = __mmask16;


class Intrinsics {
public:
#if HYP_FEATURES_INTRINSICS && !defined(HYP_ARM)
    /* Float256 Methods */
    static const HYP_FORCE_INLINE Float256 Float256Set(Float256 dest, const float *src)
        { return _mm256_load_ps(src); }
    static const HYP_FORCE_INLINE Float256 Float256Add(Float256 a, Float256 b)
        { return _mm256_add_ps(a, b); }
    static const HYP_FORCE_INLINE Float256 Float256Sub(Float256 a, Float256 b)
        { return _mm256_sub_ps(a, b); }
    static const HYP_FORCE_INLINE Float256 Float256Mul(Float256 a, Float256 b)
        { return _mm256_mul_ps(a, b); }
    static const HYP_FORCE_INLINE Float256 Float256Div(Float256 a, Float256 b)
        { return _mm256_div_ps(a, b); }


    /* Float512 Methods */
    static const HYP_FORCE_INLINE Float512 Float512Set(Float512 dest, const float *src)
        { return _mm512_load_ps(src); }
    static const HYP_FORCE_INLINE Float512 Float512Add(Float512 a, Float512 b)
        { return _mm512_add_ps(a, b); }
    static const HYP_FORCE_INLINE Float512 Float512Sub(Float512 a, Float512 b)
        { return _mm512_sub_ps(a, b); }
    static const HYP_FORCE_INLINE Float512 Float512Mul(Float512 a, Float512 b)
        { return _mm512_mul_ps(a, b); }
    static const HYP_FORCE_INLINE Float512 Float512Div(Float512 a, Float512 b)
        { return _mm512_div_ps(a, b); }

    /* There are no direct AVX-512 instructions for these, so only
     * use in instances we need them! */
    static const HYP_FORCE_INLINE Float512 Float512Sqrt(Float512 a)
        { return _mm512_sqrt_ps(a); }
#else
    /* Float256 Methods */
    static const HYP_FORCE_INLINE Float256 Float256Set(Float256 dest, const float *src)
    {
        const Float256 retv;
        Memory::Copy(retv.values, src, sizeof(retv.values));
        return retv;
    }
    static const HYP_FORCE_INLINE Float256 Float256Add(Float256 a, Float256 b)
    {
        return {
            a[0]+b[0], a[1]+b[1], a[2]+b[2], a[3]+b[3],
            a[4]+b[4], a[5]+b[5], a[6]+b[6], a[7]+b[7]
        };
    }
    static HYP_FORCE_INLINE Float256 Float256Sub(Float256 a, Float256 b)
    {
        return {
            a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3],
            a[4]-b[4], a[5]-b[5], a[6]-b[6], a[7]-b[7]
        };
    }
    static const HYP_FORCE_INLINE Float256 Float256Mul(Float256 a, Float256 b)
    {
        return {
            a[0]*b[0], a[1]*b[1], a[2]*b[2], a[3]*b[3],
            a[4]*b[4], a[5]*b[5], a[6]*b[6], a[7]*b[7]
        };
    }
    static const HYP_FORCE_INLINE Float256 Float256Div(Float256 a, Float256 b)
    {
        return {
            a[0]/b[0], a[1]/b[1], a[2]/b[2], a[3]/b[3],
            a[4]/b[4], a[5]/b[5], a[6]/b[6], a[7]/b[7]
        };
    }


    /*
     * Float512 Methods
     */
    static const HYP_FORCE_INLINE Float512 Float512Set(Float512 dest, const float *src)
    {
        Float512 retv;
        Memory::Copy(retv.values, src, sizeof(retv.values));
        return retv;
    }
    static const HYP_FORCE_INLINE Float512 Float512Add(Float512 a, Float512 b)
    {
        return {
            a[0]+b[0],   a[1]+b[1],   a[2]+b[2],   a[3]+b[3],
            a[4]+b[4],   a[5]+b[5],   a[6]+b[6],   a[7]+b[7],
            a[8]+b[8],   a[9]+b[9],   a[10]+b[10], a[11]+b[11],
            a[12]+b[12], a[13]+b[13], a[14]+b[14], a[15]+b[15]
        };
    }
    static const HYP_FORCE_INLINE Float512 Float512Sub(Float512 a, Float512 b)
    {
        return {
            a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3],
            a[4]-b[4], a[5]-b[5], a[6]-b[6], a[7]-b[7],
            a[8]-b[8],   a[9]-b[9],   a[10]-b[10], a[11]-b[11],
            a[12]-b[12], a[13]-b[13], a[14]-b[14], a[15]-b[15]
        };
    }
    static const HYP_FORCE_INLINE Float512 Float512Mul(Float512 a, Float512 b)
    {
        return {
            a[0]*b[0],   a[1]*b[1],   a[2]*b[2],   a[3]*b[3],
            a[4]*b[4],   a[5]*b[5],   a[6]*b[6],   a[7]*b[7],
            a[8]*b[8],   a[9]*b[9],   a[10]*b[10], a[11]*b[11],
            a[12]*b[12], a[13]*b[13], a[14]*b[14], a[15]*b[15]
        };
    }
    static const HYP_FORCE_INLINE Float512 Float512Div(Float512 a, Float512 b)
    {
        return {
            a[0]/b[0],   a[1]/b[1],   a[2]/b[2],   a[3]/b[3],
            a[4]/b[4],   a[5]/b[5],   a[6]/b[6],   a[7]/b[7],
            a[8]/b[8],   a[9]/b[9],   a[10]/b[10], a[11]/b[11],
            a[12]/b[12], a[13]/b[13], a[14]/b[14], a[15]/b[15]
        };
    }
#endif
};




};

#endif //HYPERION_INTRINSICS_HPP
