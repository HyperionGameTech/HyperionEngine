#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AssetPath Reflection Data

HYP_BEGIN_STRUCT(AssetPath, 263, 0, {}, ClassAttribute("size", 8))
    Property(NAME(HYP_STR(Value)), &AssetPath::ToString, &AssetPath::Set),
    Field(NAME(HYP_STR(Chain)), &AssetPath::chain, offsetof(AssetPath, chain), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(IsValid)), &AssetPath::IsValid),
    Method(NAME(HYP_STR(GetName)), &AssetPath::GetName),
    Method(NAME(HYP_STR(GetChain)), &AssetPath::GetChain),
    Method(NAME(HYP_STR(SetChain)), &AssetPath::SetChain),
    Method(NAME(HYP_STR(ToString)), &AssetPath::ToString)
HYP_END_STRUCT

#pragma endregion AssetPath Reflection Data

static_assert(sizeof(AssetPath) == 8, "Expected sizeof(AssetPath) to be 8 bytes");
} // namespace hyperion

