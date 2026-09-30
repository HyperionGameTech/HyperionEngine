/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector4.hpp>
#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

HYP_FORCE_INLINE uint32 GlimmerHashUint(uint32 value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;

    return value;
}

/*! \brief Uniformly random rotation quaternion (Shoemake), so each update's ray set samples new directions.
 *  Pairs with GlimmerRotateByQuaternion in Shaders/Glimmer/GlimmerCommon.hlsli. */
HYP_FORCE_INLINE Vec4f MakeGlimmerRandomRotation(uint32 seed)
{
    const float u1 = float(GlimmerHashUint(seed * 3 + 0) & 0xFFFFFFu) / float(0x1000000);
    const float u2 = float(GlimmerHashUint(seed * 3 + 1) & 0xFFFFFFu) / float(0x1000000);
    const float u3 = float(GlimmerHashUint(seed * 3 + 2) & 0xFFFFFFu) / float(0x1000000);

    const float a = MathUtil::Sqrt(1.0f - u1);
    const float b = MathUtil::Sqrt(u1);
    const float twoPi = 2.0f * MathUtil::pi<float>;

    return Vec4f(a * MathUtil::Sin(twoPi * u2), a * MathUtil::Cos(twoPi * u2), b * MathUtil::Sin(twoPi * u3), b * MathUtil::Cos(twoPi * u3));
}

} // namespace Hyperion
