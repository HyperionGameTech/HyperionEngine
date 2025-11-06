#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Mat4f Reflection Data

HYP_BEGIN_STRUCT(Mat4f, 243, 0, {}, ClassAttribute("size", 64))
HYP_END_STRUCT

#pragma endregion Mat4f Reflection Data

static_assert(sizeof(Mat4f) == 64, "Expected sizeof(Mat4f) to be 64 bytes");
} // namespace hyperion

