#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Mesh Reflection Data

HYP_BEGIN_CLASS(Mesh, 22, 0, NAME("AssetObject"))
    HypMethod(NAME(HYP_STR(GetFlags)), &Mesh::GetFlags),
    HypMethod(NAME(HYP_STR(SetFlags)), &Mesh::SetFlags),
    HypMethod(NAME(HYP_STR(Rename)), &Mesh::Rename),
    HypMethod(NAME(HYP_STR(NumIndices)), &Mesh::NumIndices),
    HypMethod(NAME(HYP_STR(GetVertexAttributes)), &Mesh::GetVertexAttributes, Span<const HypClassAttribute> { {HypClassAttribute("property", "VertexAttributes"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetTopology)), &Mesh::GetTopology, Span<const HypClassAttribute> { {HypClassAttribute("property", "Topology"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(GetAABB)), &Mesh::GetAABB, Span<const HypClassAttribute> { {HypClassAttribute("property", "AABB"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetAABB)), &Mesh::SetAABB, Span<const HypClassAttribute> { {HypClassAttribute("property", "AABB"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetMeshAsset)), &Mesh::GetMeshAsset, Span<const HypClassAttribute> { {HypClassAttribute("property", "MeshAsset") } }),
    HypMethod(NAME(HYP_STR(SetMeshAsset)), &Mesh::SetMeshAsset, Span<const HypClassAttribute> { {HypClassAttribute("property", "MeshAsset") } }),
    HypField(NAME(HYP_STR(Aabb)), &Mesh::m_aabb, offsetof(Mesh, m_aabb), Span<const HypClassAttribute> { {HypClassAttribute("property", "AABB") } }),
    HypField(NAME(HYP_STR(Bvh)), &Mesh::m_bvh, offsetof(Mesh, m_bvh), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Flags)), &Mesh::m_flags, offsetof(Mesh, m_flags))
HYP_END_CLASS

#pragma endregion Mesh Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshFlags Reflection Data

HYP_BEGIN_ENUM(MeshFlags, 274, 0, {})
    HypConstant(NAME(HYP_STR(MF_NONE)), MeshFlags::MF_NONE),
    HypConstant(NAME(HYP_STR(MF_VIEW_INDEPENDENT)), MeshFlags::MF_VIEW_INDEPENDENT)
HYP_END_ENUM

#pragma endregion MeshFlags Reflection Data

} // namespace hyperion

