#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Quaternion Reflection Data

HYP_BEGIN_STRUCT(Quaternion, 243, 0, {}, HypClassAttribute("size", 16))
    HypField(NAME(HYP_STR(X)), &Quaternion::x, offsetof(Quaternion, x)),
    HypField(NAME(HYP_STR(Y)), &Quaternion::y, offsetof(Quaternion, y)),
    HypField(NAME(HYP_STR(Z)), &Quaternion::z, offsetof(Quaternion, z)),
    HypField(NAME(HYP_STR(W)), &Quaternion::w, offsetof(Quaternion, w))
HYP_END_STRUCT

#pragma endregion Quaternion Reflection Data

static_assert(sizeof(Quaternion) == 16, "Expected sizeof(Quaternion) to be 16 bytes");
} // namespace hyperion

