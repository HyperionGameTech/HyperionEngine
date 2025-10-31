#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AssetPath Reflection Data

HYP_BEGIN_STRUCT(AssetPath, 262, 0, {}, HypClassAttribute("size", 8))
    HypProperty(NAME(HYP_STR(Value)), &AssetPath::ToString, &AssetPath::Set),
    HypField(NAME(HYP_STR(Chain)), &AssetPath::chain, offsetof(AssetPath, chain), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(IsValid)), &AssetPath::IsValid),
    HypMethod(NAME(HYP_STR(GetName)), &AssetPath::GetName),
    HypMethod(NAME(HYP_STR(GetChain)), &AssetPath::GetChain),
    HypMethod(NAME(HYP_STR(SetChain)), &AssetPath::SetChain),
    HypMethod(NAME(HYP_STR(ToString)), &AssetPath::ToString)
HYP_END_STRUCT

#pragma endregion AssetPath Reflection Data

static_assert(sizeof(AssetPath) == 8, "Expected sizeof(AssetPath) to be 8 bytes");
} // namespace hyperion

