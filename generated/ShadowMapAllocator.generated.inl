#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ShadowMapAtlas Reflection Data

HYP_BEGIN_STRUCT(ShadowMapAtlas, 355, 0, {})
    Property(NAME(HYP_STR(AtlasDimensions)), &ShadowMapAtlas::atlasDimensions),
    Property(NAME(HYP_STR(Elements)), &ShadowMapAtlas::elements),
    Property(NAME(HYP_STR(FreeSpaces)), &ShadowMapAtlas::freeSpaces),
    Field(NAME(HYP_STR(AtlasIndex)), &ShadowMapAtlas::atlasIndex, offsetof(ShadowMapAtlas, atlasIndex), Span<const ClassAttribute> { {ClassAttribute("property", "AtlasIndex"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion ShadowMapAtlas Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShadowMapAtlasElement Reflection Data

HYP_BEGIN_STRUCT(ShadowMapAtlasElement, 356, 0, {})
HYP_END_STRUCT

#pragma endregion ShadowMapAtlasElement Reflection Data

} // namespace hyperion

