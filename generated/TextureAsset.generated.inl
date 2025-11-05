#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region TextureAsset Reflection Data

HYP_BEGIN_CLASS(TextureAsset, 47, 0, NAME("AssetObject"))
    Field(NAME(HYP_STR(TextureDesc)), &TextureAsset::m_textureDesc, offsetof(TextureAsset, m_textureDesc), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion TextureAsset Reflection Data

} // namespace hyperion

