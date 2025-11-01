#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region MeshData Reflection Data

HYP_BEGIN_STRUCT(MeshData, 222, 0, {})
    HypField(NAME(HYP_STR(VertexData)), &MeshData::vertexData, offsetof(MeshData, vertexData), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("compressed", true) } }),
    HypField(NAME(HYP_STR(IndexData)), &MeshData::indexData, offsetof(MeshData, indexData), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("compressed", true) } })
HYP_END_STRUCT

#pragma endregion MeshData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshAsset Reflection Data

HYP_BEGIN_CLASS(MeshAsset, 17, 0, NAME("AssetObject"))
    HypField(NAME(HYP_STR(MeshDesc)), &MeshAsset::m_meshDesc, offsetof(MeshAsset, m_meshDesc), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion MeshAsset Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region MeshDesc Reflection Data

HYP_BEGIN_STRUCT(MeshDesc, 223, 0, {})
    HypField(NAME(HYP_STR(MeshAttributes)), &MeshDesc::meshAttributes, offsetof(MeshDesc, meshAttributes), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(NumVertices)), &MeshDesc::numVertices, offsetof(MeshDesc, numVertices), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(NumIndices)), &MeshDesc::numIndices, offsetof(MeshDesc, numIndices), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion MeshDesc Reflection Data

} // namespace hyperion

