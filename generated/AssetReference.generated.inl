#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AssetReference Reflection Data

HYP_BEGIN_STRUCT(AssetReference, 260, 0, {}, HypClassAttribute("size", 16))
    HypMethod(NAME(HYP_STR(GetAssetPath)), &AssetReference::GetAssetPath, Span<const HypClassAttribute> { {HypClassAttribute("property", "AssetPath"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetAssetPath)), &AssetReference::SetAssetPath, Span<const HypClassAttribute> { {HypClassAttribute("property", "AssetPath"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion AssetReference Reflection Data

static_assert(sizeof(AssetReference) == 16, "Expected sizeof(AssetReference) to be 16 bytes");
} // namespace hyperion

