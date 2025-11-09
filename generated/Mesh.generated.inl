#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Mesh Reflection Data

HYP_BEGIN_CLASS(Mesh, 22, 0, NAME("AssetObject"))
    Method(NAME(HYP_STR(GetFlags)), &Mesh::GetFlags),
    Method(NAME(HYP_STR(SetFlags)), &Mesh::SetFlags),
    Method(NAME(HYP_STR(Rename)), &Mesh::Rename),
    Method(NAME(HYP_STR(NumIndices)), &Mesh::NumIndices),
    Method(NAME(HYP_STR(GetVertexAttributes)), &Mesh::GetVertexAttributes, Span<const ClassAttribute> { {ClassAttribute("property", "VertexAttributes"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetTopology)), &Mesh::GetTopology, Span<const ClassAttribute> { {ClassAttribute("property", "Topology"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(GetAABB)), &Mesh::GetAABB, Span<const ClassAttribute> { {ClassAttribute("property", "AABB"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetAABB)), &Mesh::SetAABB, Span<const ClassAttribute> { {ClassAttribute("property", "AABB"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetMeshAsset)), &Mesh::GetMeshAsset, Span<const ClassAttribute> { {ClassAttribute("property", "MeshAsset") } }),
    Method(NAME(HYP_STR(SetMeshAsset)), &Mesh::SetMeshAsset, Span<const ClassAttribute> { {ClassAttribute("property", "MeshAsset") } }),
    Field(NAME(HYP_STR(Aabb)), &Mesh::m_aabb, offsetof(Mesh, m_aabb), Span<const ClassAttribute> { {ClassAttribute("property", "AABB") } }),
    Field(NAME(HYP_STR(Bvh)), &Mesh::m_bvh, offsetof(Mesh, m_bvh), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Flags)), &Mesh::m_flags, offsetof(Mesh, m_flags))
HYP_END_CLASS

#pragma endregion Mesh Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshFlags Reflection Data

HYP_BEGIN_ENUM(MeshFlags, 282, 0, {})
    StaticField(NAME(HYP_STR(MF_NONE)), MeshFlags::MF_NONE),
    StaticField(NAME(HYP_STR(MF_VIEW_INDEPENDENT)), MeshFlags::MF_VIEW_INDEPENDENT)
HYP_END_ENUM

#pragma endregion MeshFlags Reflection Data

} // namespace hyperion

