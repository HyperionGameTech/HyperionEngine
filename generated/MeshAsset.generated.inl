#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region MeshData Reflection Data

HYP_BEGIN_STRUCT(MeshData, 269, 0, {})
    Field(NAME(HYP_STR(VertexData)), &MeshData::vertexData, offsetof(MeshData, vertexData), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("compressed", true) } }),
    Field(NAME(HYP_STR(IndexData)), &MeshData::indexData, offsetof(MeshData, indexData), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("compressed", true) } })
HYP_END_STRUCT

#pragma endregion MeshData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshAsset Reflection Data

HYP_BEGIN_CLASS(MeshAsset, 50, 0, NAME("AssetObject"))
    Field(NAME(HYP_STR(MeshDesc)), &MeshAsset::m_meshDesc, offsetof(MeshAsset, m_meshDesc), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion MeshAsset Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshDesc Reflection Data

HYP_BEGIN_STRUCT(MeshDesc, 270, 0, {})
    Field(NAME(HYP_STR(MeshAttributes)), &MeshDesc::meshAttributes, offsetof(MeshDesc, meshAttributes), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(NumVertices)), &MeshDesc::numVertices, offsetof(MeshDesc, numVertices), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(NumIndices)), &MeshDesc::numIndices, offsetof(MeshDesc, numIndices), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MeshDesc Reflection Data

} // namespace hyperion

