#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Texture Reflection Data

HYP_BEGIN_CLASS(Texture, 51, 0, NAME("AssetObject"))
    Method(NAME(HYP_STR(Rename)), &Texture::Rename),
    Method(NAME(HYP_STR(GetAssetReference)), &Texture::GetAssetReference, Span<const ClassAttribute> { {ClassAttribute("property", "AssetReference") } }),
    Method(NAME(HYP_STR(SetAssetReference)), &Texture::SetAssetReference, Span<const ClassAttribute> { {ClassAttribute("property", "AssetReference") } }),
    Field(NAME(HYP_STR(GpuImage)), &Texture::m_gpuImage, offsetof(Texture, m_gpuImage), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion Texture Reflection Data

} // namespace hyperion

