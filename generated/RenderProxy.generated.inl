#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region MeshRaytracingData Reflection Data

HYP_BEGIN_STRUCT(MeshRaytracingData, 291, 0, {})
    Field(NAME(HYP_STR(Blas)), &MeshRaytracingData::blas, offsetof(MeshRaytracingData, blas), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion MeshRaytracingData Reflection Data

} // namespace hyperion

