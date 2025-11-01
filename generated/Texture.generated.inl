#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Texture Reflection Data

HYP_BEGIN_CLASS(Texture, 23, 0, NAME("AssetObject"))
    HypMethod(NAME(HYP_STR(Rename)), &Texture::Rename),
    HypMethod(NAME(HYP_STR(GetAssetReference)), &Texture::GetAssetReference, Span<const HypClassAttribute> { {HypClassAttribute("property", "AssetReference") } }),
    HypMethod(NAME(HYP_STR(SetAssetReference)), &Texture::SetAssetReference, Span<const HypClassAttribute> { {HypClassAttribute("property", "AssetReference") } }),
    HypField(NAME(HYP_STR(GpuImage)), &Texture::m_gpuImage, offsetof(Texture, m_gpuImage), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion Texture Reflection Data

} // namespace hyperion

