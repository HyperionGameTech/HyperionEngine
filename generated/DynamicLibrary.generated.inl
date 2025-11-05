#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region DynamicLibrary Reflection Data

HYP_BEGIN_STRUCT(DynamicLibrary, 254, 0, {}, ClassAttribute("size", 8))
    Method(NAME(HYP_STR(GetPath)), &DynamicLibrary::GetPath),
    Method(NAME(HYP_STR(SetPath)), &DynamicLibrary::SetPath),
    Method(NAME(HYP_STR(Load)), &DynamicLibrary::Load),
    Method(NAME(HYP_STR(GetFunction)), &DynamicLibrary::GetFunction)
HYP_END_STRUCT

#pragma endregion DynamicLibrary Reflection Data

static_assert(sizeof(DynamicLibrary) == 8, "Expected sizeof(DynamicLibrary) to be 8 bytes");
} // namespace hyperion

