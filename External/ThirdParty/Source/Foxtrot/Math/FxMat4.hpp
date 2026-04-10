#pragma once

#include "FxVec3.hpp"
#include "FxVec4.hpp"


#if FX_USE_NEON
#include <arm_neon.h>
#elif FX_USE_AVX
#include "FxSSE.hpp"
#endif

class FxQuat;

/**
 * @brief A single-precision 4x4 matrix class using SIMD instructions.
 *
 * This class represents a 4x4 matrix represented using _Column-Major_ order to match
 * the layout of MSL, GLSL, and _modern_ HLSL.
 */
class alignas(16) FxMat4f
{
public:
    static const FxMat4f sIdentity;

public:
    FxMat4f() noexcept
    {
        Columns[0].Set(0);
        Columns[1].Set(0);
        Columns[2].Set(0);
        Columns[3].Set(0);
    }

    FxMat4f(const float data[16]) noexcept
    {
        Columns[0] = FxVec4f(data);
        Columns[1] = FxVec4f(data + 4);
        Columns[2] = FxVec4f(data + 8);
        Columns[3] = FxVec4f(data + 12);
    }

    FxMat4f(float data[4][4]) noexcept;


    static FxMat4f FromRows(float data[16])
    {
        return FxMat4f(FxVec4f(data[0], data[4], data[8], data[12]), FxVec4f(data[1], data[5], data[9], data[13]),
                       FxVec4f(data[2], data[6], data[10], data[14]), FxVec4f(data[3], data[7], data[11], data[15]));
    }

    static FxMat4f AsTranslation(FxVec3f position)
    {
        FxMat4f result = FxMat4f::sIdentity;
        result.Columns[3].Set(position.X, position.Y, position.Z, 1.0f);
        return result;

        /*return FxMat4f((float32[16]) {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            position.X, position.Y, position.Z, 1 });*/
    }

    static FxMat4f AsScale(FxVec3f scale)
    {
        FxMat4f result = FxMat4f::sIdentity;

        result.Columns[0] *= scale.X;
        result.Columns[1] *= scale.Y;
        result.Columns[2] *= scale.Z;

        return result;

        // return FxMat4f((float32[16]) { scale.X, 0, 0, 0, 0, scale.Y, 0, 0, 0, 0, scale.Z, 0, 0, 0, 0, 1 });
    }

    static FxMat4f AsRotationX(float rad);
    static FxMat4f AsRotationY(float rad);
    static FxMat4f AsRotationZ(float rad);

    static FxMat4f AsRotation(const FxQuat& quat);

    void Rotate(FxVec3f rotation);

    void LookAt(FxVec3f position, FxVec3f target, FxVec3f up);

    FxMat4f(float scalar) noexcept
    {
        Columns[0].Set(scalar);
        Columns[1].Set(scalar);
        Columns[2].Set(scalar);
        Columns[3].Set(scalar);
    }

    FxMat4f(FxVec4f c0, FxVec4f c1, FxVec4f c2, FxVec4f c3) noexcept
    {
        Columns[0] = c0;
        Columns[1] = c1;
        Columns[2] = c2;
        Columns[3] = c3;
    }

    FxMat4f(FxVec4f columns[4]) noexcept
    {
        Columns[0] = columns[0];
        Columns[1] = columns[1];
        Columns[2] = columns[2];
        Columns[3] = columns[3];
    }

    inline void LoadIdentity()
    {
        Columns[0].Set(1.0f, 0.0f, 0.0f, 0.0f);
        Columns[1].Set(0.0f, 1.0f, 0.0f, 0.0f);
        Columns[2].Set(0.0f, 0.0f, 1.0f, 0.0f);
        Columns[3].Set(0.0f, 0.0f, 0.0f, 1.0f);
    }

    void Set(float data[16])
    {
        Columns[0] = FxVec4f(data);
        Columns[1] = FxVec4f(data + 4);
        Columns[2] = FxVec4f(data + 8);
        Columns[3] = FxVec4f(data + 12);
    }

    FxMat4f& operator=(const FxMat4f& other)
    {
        Columns[0] = other.Columns[0];
        Columns[1] = other.Columns[1];
        Columns[2] = other.Columns[2];
        Columns[3] = other.Columns[3];
        return *this;
    }

    FxMat4f Inverse() const;
    FxMat4f Transposed() const;
    FxMat4f TransposeMat3();

    void LoadPerspectiveMatrix(float32 hfov, float32 aspect_ratio, float32 near_plane, float32 far_plane);
    void LoadOrthographicMatrix(float32 width, float32 height, float32 near_plane, float32 far_plane);

    void CopyAsMat3To(float* dest) const;

    FxMat4f operator*(const FxMat4f& other) const;

    void Print()
    {
        printf("\t=== Matrix ===\n");
        for (int i = 0; i < 4; i++) {
            printf("{ %.06f\t%.06f\t%.06f\t%.06f }", Columns[0].mData[i], Columns[1].mData[i], Columns[2].mData[i],
                   Columns[3].mData[i]);
            putchar('\n');
        }
    }

    // Mat4f Multiply(const Mat4f &other);
    FxVec4f MultiplyVec4f(FxVec4f& vec);

    FxMat4f GetWithoutTranslation() const;

public:
#if defined(FX_USE_NEON)
    float32x4_t MultiplyVec4f_Neon(FxVec4f& vec);
#elif defined(FX_USE_AVX)
    __m128 MultiplyVec4f_SSE(const FxVec4f& vec);
#endif

public:
    union alignas(16)
    {
        FxVec4f Columns[4];
        float32 RawData[16];
    };

    friend class FxVec4f;

private:
};
