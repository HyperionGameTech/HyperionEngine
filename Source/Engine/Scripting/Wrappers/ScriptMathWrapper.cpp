/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#ifdef HYP_SCRIPT

#include <Core/Math/MathUtil.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

namespace Hyperion {

ENGINE_API const Class* g_clsMath = nullptr;

class MathDummyClass {};
using Math = MathDummyClass;

// clang-format off
HYP_BEGIN_STRUCT(Math, -1, 0, {})
    Method(NAME("Sin"), +[](float x) -> float
        {
            return MathUtil::Sin(x);
        }),
    Method(NAME("Cos"), +[](float x) -> float
        {
            return MathUtil::Cos(x);
        }),
    Method(NAME("Tan"), +[](float x) -> float
        {
            return MathUtil::Tan(x);
        }),
    Method(NAME("Arcsin"), +[](float x) -> float
        {
            return MathUtil::Arcsin(x);
        }),
    Method(NAME("Arccos"), +[](float x) -> float
        {
            return MathUtil::Arccos(x);
        }),
    Method(NAME("Arctan"), +[](float x) -> float
        {
            return MathUtil::Arctan(x);
        }),
    Method(NAME("RadToDeg"), +[](float rad) -> float
        {
            return MathUtil::RadToDeg(rad);
        }),
    Method(NAME("DegToRad"), +[](float deg) -> float
        {
            return MathUtil::DegToRad(deg);
        }),
    Method(NAME("Sqrt"), +[](float value) -> float
        {
            return MathUtil::Sqrt<float>(value);
        }),
    Method(NAME("Pow"), +[](float value, float exponent) -> float
        {
            return MathUtil::Pow(value, exponent);
        }),
    Method(NAME("Abs"), +[](float value) -> float
        {
            return MathUtil::Abs(value);
        }),
    Method(NAME("Min"), +[](float a, float b) -> float
        {
            return MathUtil::Min(a, b);
        }),
    Method(NAME("Max"), +[](float a, float b) -> float
        {
            return MathUtil::Max(a, b);
        }),
    Method(NAME("Clamp"), +[](float val, float min, float max) -> float
        {
            return MathUtil::Clamp(val, min, max);
        }),
    Method(NAME("Floor"), +[](float value) -> float
        {
            return MathUtil::Floor<float, float>(value);
        }),
    Method(NAME("Ceil"), +[](float value) -> float
        {
            return MathUtil::Ceil<float, float>(value);
        }),
    Method(NAME("Trunc"), +[](float value) -> float
        {
            return static_cast<float>(MathUtil::Trunc<float>(value));
        }),
    Method(NAME("Round"), +[](float value) -> float
        {
            return MathUtil::Round<float>(value);
        }),
    Method(NAME("Fract"), +[](float value) -> float
        {
            return MathUtil::Fract(value);
        }),
    Method(NAME("Sign"), +[](float value) -> int
        {
            return MathUtil::Sign<float, int>(value);
        }),
    Method(NAME("Lerp"), +[](float from, float to, float amt) -> float
        {
            return MathUtil::Lerp(from, to, amt);
        }),
    Method(NAME("Step"), +[](float edge, float x) -> float
        {
            return MathUtil::Step(edge, x);
        }),
    Method(NAME("Exp"), +[](float value) -> float
        {
            return MathUtil::Exp(value);
        }),
    Method(NAME("Mod"), +[](float a, float b) -> float
        {
            return MathUtil::Mod(a, b);
        }),
    Method(NAME("IsNaN"), +[](float value) -> bool
        {
            return MathUtil::IsNaN(value);
        }),
    Method(NAME("IsFinite"), +[](float value) -> bool
        {
            return MathUtil::IsFinite(value);
        }),
    Method(NAME("ApproxEqual"), +[](float a, float b) -> bool
        {
            return MathUtil::ApproxEqual(a, b);
        }),
    Method(NAME("Factorial"), +[](float value) -> float
        {
            return MathUtil::Factorial(value);
        })
HYP_END_STRUCT
// clang-format on

HYP_REGISTER_STATIC_CLASS(Math);

} // namespace Hyperion

#endif
