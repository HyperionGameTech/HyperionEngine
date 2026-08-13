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

} // namespace Hyperion

#pragma pack(pop)