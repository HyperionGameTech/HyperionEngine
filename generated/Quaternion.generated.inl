#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Quaternion Reflection Data

HYP_BEGIN_STRUCT(Quaternion, 245, 0, {}, ClassAttribute("size", 16))
    Field(NAME(HYP_STR(X)), &Quaternion::x, offsetof(Quaternion, x)),
    Field(NAME(HYP_STR(Y)), &Quaternion::y, offsetof(Quaternion, y)),
    Field(NAME(HYP_STR(Z)), &Quaternion::z, offsetof(Quaternion, z)),
    Field(NAME(HYP_STR(W)), &Quaternion::w, offsetof(Quaternion, w))
HYP_END_STRUCT

#pragma endregion Quaternion Reflection Data

static_assert(sizeof(Quaternion) == 16, "Expected sizeof(Quaternion) to be 16 bytes");
} // namespace hyperion

