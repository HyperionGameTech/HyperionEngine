#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region DynamicLibrary Reflection Data

HYP_BEGIN_STRUCT(DynamicLibrary, 235, 0, {}, HypClassAttribute("size", 8))
    HypMethod(NAME(HYP_STR(GetPath)), &DynamicLibrary::GetPath),
    HypMethod(NAME(HYP_STR(SetPath)), &DynamicLibrary::SetPath),
    HypMethod(NAME(HYP_STR(Load)), &DynamicLibrary::Load),
    HypMethod(NAME(HYP_STR(GetFunction)), &DynamicLibrary::GetFunction)
HYP_END_STRUCT

#pragma endregion DynamicLibrary Reflection Data

static_assert(sizeof(DynamicLibrary) == 8, "Expected sizeof(DynamicLibrary) to be 8 bytes");
} // namespace hyperion

