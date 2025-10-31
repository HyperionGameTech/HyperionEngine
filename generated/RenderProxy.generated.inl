#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region MeshRaytracingData Reflection Data

HYP_BEGIN_STRUCT(MeshRaytracingData, 337, 0, {})
    HypField(NAME(HYP_STR(Blas)), &MeshRaytracingData::blas, offsetof(MeshRaytracingData, blas), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion MeshRaytracingData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region EnvProbeSphericalHarmonics Reflection Data

HYP_BEGIN_STRUCT(EnvProbeSphericalHarmonics, 338, 0, {}, HypClassAttribute("serialize", "bitwise"))
HYP_END_STRUCT

#pragma endregion EnvProbeSphericalHarmonics Reflection Data

} // namespace hyperion

