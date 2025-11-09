#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AssetReference Reflection Data

HYP_BEGIN_STRUCT(AssetReference, 266, 0, {}, ClassAttribute("size", 16))
    Method(NAME(HYP_STR(GetAssetPath)), &AssetReference::GetAssetPath, Span<const ClassAttribute> { {ClassAttribute("property", "AssetPath"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetAssetPath)), &AssetReference::SetAssetPath, Span<const ClassAttribute> { {ClassAttribute("property", "AssetPath"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion AssetReference Reflection Data

static_assert(sizeof(AssetReference) == 16, "Expected sizeof(AssetReference) to be 16 bytes");
} // namespace hyperion

