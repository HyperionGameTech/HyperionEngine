#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Error Reflection Data

HYP_BEGIN_STRUCT(Error, 251, 0, {}, HypClassAttribute("size", 16))
HYP_END_STRUCT

#pragma endregion Error Reflection Data

static_assert(sizeof(Error) == 16, "Expected sizeof(Error) to be 16 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region Result Reflection Data

HYP_BEGIN_STRUCT(Result, 252, 0, {}, HypClassAttribute("size", 8))
    HypMethod(NAME(HYP_STR(HasValue)), &Result::HasValue),
    HypMethod(NAME(HYP_STR(HasError)), &Result::HasError),
    HypMethod(NAME(HYP_STR(GetError)), &Result::GetError)
HYP_END_STRUCT

#pragma endregion Result Reflection Data

static_assert(sizeof(Result) == 8, "Expected sizeof(Result) to be 8 bytes");
} // namespace hyperion

