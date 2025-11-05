#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Mat3f Reflection Data

HYP_BEGIN_STRUCT(Mat3f, 239, 0, {}, ClassAttribute("size", 48))
HYP_END_STRUCT

#pragma endregion Mat3f Reflection Data

static_assert(sizeof(Mat3f) == 48, "Expected sizeof(Mat3f) to be 48 bytes");
} // namespace hyperion

