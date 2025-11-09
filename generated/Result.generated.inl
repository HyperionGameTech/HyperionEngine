#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Error Reflection Data

HYP_BEGIN_STRUCT(Error, 253, 0, {}, ClassAttribute("size", 16))
HYP_END_STRUCT

#pragma endregion Error Reflection Data

static_assert(sizeof(Error) == 16, "Expected sizeof(Error) to be 16 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region Result Reflection Data

HYP_BEGIN_STRUCT(Result, 254, 0, {}, ClassAttribute("size", 8))
    Method(NAME(HYP_STR(HasValue)), &Result::HasValue),
    Method(NAME(HYP_STR(HasError)), &Result::HasError),
    Method(NAME(HYP_STR(GetError)), &Result::GetError)
HYP_END_STRUCT

#pragma endregion Result Reflection Data

static_assert(sizeof(Result) == 8, "Expected sizeof(Result) to be 8 bytes");
} // namespace hyperion

