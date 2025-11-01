#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region TextureAsset Reflection Data

HYP_BEGIN_CLASS(TextureAsset, 20, 0, NAME("AssetObject"))
    HypField(NAME(HYP_STR(TextureDesc)), &TextureAsset::m_textureDesc, offsetof(TextureAsset, m_textureDesc), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion TextureAsset Reflection Data

} // namespace hyperion

