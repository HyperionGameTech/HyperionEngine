/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Util/Img/Bitmap.hpp>

namespace Hyperion {

#pragma pack(push, 4)

HYP_STRUCT()
struct SphericalHarmonicsData
{
    HYP_STRUCT_BODY(SphericalHarmonicsData);

    float values[9 * 3];

    HYP_FORCE_INLINE bool operator==(const SphericalHarmonicsData& other) const
    {
        return std::memcmp(values, other.values, sizeof(values)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const SphericalHarmonicsData& other) const
    {
        return std::memcmp(values, other.values, sizeof(values)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(
            reinterpret_cast<const ubyte*>(values),
            reinterpret_cast<const ubyte*>(values) + sizeof(values));
    }

#pragma region Helpers
    /// Scale by weight
    HYP_NODISCARD HYP_FORCE_INLINE SphericalHarmonicsData operator*(float weight) const
    {
        SphericalHarmonicsData result;
        
        for (size_t i = 0; i < GetArrayCount(values); i++)
        {
            result.values[i] = values[i] * weight;
        }

        return result;
    }

    /// Accum
    HYP_NODISCARD HYP_FORCE_INLINE SphericalHarmonicsData operator+(const SphericalHarmonicsData& other) const
    {
        SphericalHarmonicsData result;
        
        for (size_t i = 0; i < GetArrayCount(values); i++)
        {
            result.values[i] = values[i] + other.values[i];
        }

        return result;
    }
#pragma endregion Helpers

#pragma region Serialization

    HYP_METHOD(Property = "Order0", Serialize = true, NoScriptBindings)
    Vec3f GetOrder0() const
    {
        return Vec3f(values[0], values[1], values[2]);
    }

    HYP_METHOD(Property = "Order0", Serialize = true, NoScriptBindings)
    void SetOrder0(const Vec3f& inValues)
    {
        std::memcpy(values, &inValues, sizeof(float) * 3);
    }

    HYP_METHOD(Property = "Order1", Serialize = true, NoScriptBindings)
    FixedArray<Vec3f, 3> GetOrder1() const
    {
        return {
            Vec3f(values[3], values[4], values[5]),
            Vec3f(values[6], values[7], values[8]),
            Vec3f(values[9], values[10], values[11])
        };
    }

    HYP_METHOD(Property = "Order1", Serialize = true, NoScriptBindings)
    void SetOrder1(const FixedArray<Vec3f, 3>& inValues)
    {
        std::memcpy(values + 3, inValues.Data() + 0, sizeof(float) * 3);
        std::memcpy(values + 6, inValues.Data() + 1, sizeof(float) * 3);
        std::memcpy(values + 9, inValues.Data() + 2, sizeof(float) * 3);
    }

    HYP_METHOD(Property = "Order2", Serialize = true, NoScriptBindings)
    FixedArray<Vec3f, 5> GetOrder2() const
    {
        return {
            Vec3f(values[12], values[13], values[14]),
            Vec3f(values[15], values[16], values[17]),
            Vec3f(values[18], values[19], values[20]),
            Vec3f(values[21], values[22], values[23]),
            Vec3f(values[24], values[25], values[26])
        };
    }

    HYP_METHOD(Property = "Order2", Serialize = true, NoScriptBindings)
    void SetOrder2(const FixedArray<Vec3f, 5>& inValues)
    {
        std::memcpy(values + 12, inValues.Data() + 0, sizeof(float) * 3);
        std::memcpy(values + 15, inValues.Data() + 1, sizeof(float) * 3);
        std::memcpy(values + 18, inValues.Data() + 2, sizeof(float) * 3);
        std::memcpy(values + 21, inValues.Data() + 3, sizeof(float) * 3);
        std::memcpy(values + 24, inValues.Data() + 4, sizeof(float) * 3);
    }

#pragma endregion Serialization
};

enum class EvaluateSphericalHarmonicsResult : int8
{
    Failure_OutsideOfVolume = -1,

    Success_InTetra = 0,
    Success_Fallback = 1
};

HYP_FORCE_INLINE static constexpr bool IsSuccess(EvaluateSphericalHarmonicsResult result)
{
    return int8(result) >= 0;
}

HYP_FORCE_INLINE static FixedArray<float, 9> EvaluateSphericalHarmonicsBasis(const Vec3f& direction)
{
    const float x = direction.x;
    const float y = direction.y;
    const float z = direction.z;
 
    return FixedArray<float, 9> {
        0.282095f,
        0.488603f * y,
        0.488603f * z,
        0.488603f * x,
        1.092548f * x * y,
        1.092548f * y * z,
        0.315392f * (3.0f * z * z - 1.0f),
        1.092548f * x * z,
        0.546274f * (x * x - y * y)
    };
}

HYP_FORCE_INLINE static Vec3f GetCubemapFaceDirection(uint32 faceIndex, float u, float v)
{
    // Keep in line with Texture::s_cubemapDirections
    switch (faceIndex)
    {
    case 0:     return Vec3f(1.0f, -v, -u);     // +X
    case 1:     return Vec3f(-1.0f, -v, u);     // -X
    case 2:     return Vec3f(u, 1.0f, v);       // +Y
    case 3:     return Vec3f(u, -1.0f, -v);     // -Y
    case 4:     return Vec3f(u, -v, 1.0f);      // +Z
    default:    return Vec3f(-u, -v, -1.0f);    // -Z
    }
}

template <TextureFormat Format, bool CosineWeighted>
static inline SphericalHarmonicsData ComputeSphericalHarmonicsCubemap(const Bitmap<Format>& bitmap, uint32 sampleStride = 1)
{
    static const FixedArray<float, 9> s_sphericalHarmonicsAOverPi {
        1.0f,
        2.0f / 3.0f,
        2.0f / 3.0f,
        2.0f / 3.0f,
        0.25f,
        0.25f,
        0.25f,
        0.25f,
        0.25f
    };

    SphericalHarmonicsData coefficients {};
 
    const uint32 width = bitmap.GetWidth();
    const uint32 height = bitmap.GetHeight();
 
    if (width == 0 || height == 0 || height % 6 != 0 || height / 6 != width)
    {
        return coefficients;
    }
 
    const uint32 faceSize = width;
 
    sampleStride = MathUtil::Max(sampleStride, 1u);
 
    const float texelStep = 2.0f / float(faceSize);
 
    float totalWeight = 0.0f;
 
    for (uint32 faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        for (uint32 y = 0; y < faceSize; y += sampleStride)
        {
            const float v = ((float(y) + 0.5f) * texelStep) - 1.0f;
 
            for (uint32 x = 0; x < faceSize; x += sampleStride)
            {
                const float u = ((float(x) + 0.5f) * texelStep) - 1.0f;
 
                const float r2 = 1.0f + u * u + v * v;
                const float solidAngle = (texelStep * texelStep) / (r2 * MathUtil::Sqrt(r2));
 
                const Vec3f direction = GetCubemapFaceDirection(faceIndex, u, v).Normalized();
 
                const Vec3f radiance = bitmap.GetPixelReference(x, faceIndex * faceSize + y).GetRGBA().GetXYZ();
 
                const FixedArray<float, 9> basis = EvaluateSphericalHarmonicsBasis(direction);
 
                for (uint32 i = 0; i < 9; i++)
                {
                    coefficients.values[i * 3 + 0] += radiance.x * (basis[i] * solidAngle);
                    coefficients.values[i * 3 + 1] += radiance.y * (basis[i] * solidAngle);
                    coefficients.values[i * 3 + 2] += radiance.z * (basis[i] * solidAngle);
                }
 
                totalWeight += solidAngle;
            }
        }
    }
 
    if (totalWeight <= MathUtil::epsilonF)
    {
        return coefficients;
    }
 
    const float normalization = (4.0f * MathUtil::pi<float>) / totalWeight;
 
    for (uint32 i = 0; i < 9; i++)
    {
        coefficients.values[i * 3 + 0] *= normalization * (CosineWeighted ? s_sphericalHarmonicsAOverPi[i] : 1.0f);
        coefficients.values[i * 3 + 1] *= normalization * (CosineWeighted ? s_sphericalHarmonicsAOverPi[i] : 1.0f);
        coefficients.values[i * 3 + 2] *= normalization * (CosineWeighted ? s_sphericalHarmonicsAOverPi[i] : 1.0f);
    }
 
    return coefficients;
}
 
HYP_FORCE_INLINE static Vec3f EvaluateSphericalHarmonics(const SphericalHarmonicsData& coefficients, const Vec3f& normal)
{
    const FixedArray<float, 9> basis = EvaluateSphericalHarmonicsBasis(normal);
 
    Vec3f result = Vec3f::Zero();
 
    for (uint32 i = 0; i < 9; i++)
    {
        result.x += coefficients.values[i * 3 + 0] * basis[i];
        result.y += coefficients.values[i * 3 + 1] * basis[i];
        result.z += coefficients.values[i * 3 + 2] * basis[i];
    }
 
    return MathUtil::Max(result, Vec3f::Zero());
}

} // namespace Hyperion

#pragma pack(pop)