#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Vertex Reflection Data

HYP_BEGIN_STRUCT(Vertex, 236, 0, {}, HypClassAttribute("size", 128),HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(Position)), &Vertex::position, offsetof(Vertex, position), Span<const HypClassAttribute> { {HypClassAttribute("property", "Position"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Normal)), &Vertex::normal, offsetof(Vertex, normal), Span<const HypClassAttribute> { {HypClassAttribute("property", "Normal"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Tangent)), &Vertex::tangent, offsetof(Vertex, tangent), Span<const HypClassAttribute> { {HypClassAttribute("property", "Tangent"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Bitangent)), &Vertex::bitangent, offsetof(Vertex, bitangent), Span<const HypClassAttribute> { {HypClassAttribute("property", "Bitangent"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Texcoord0)), &Vertex::texcoord0, offsetof(Vertex, texcoord0), Span<const HypClassAttribute> { {HypClassAttribute("property", "TexCoord0"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Texcoord1)), &Vertex::texcoord1, offsetof(Vertex, texcoord1), Span<const HypClassAttribute> { {HypClassAttribute("property", "TexCoord1"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(BoneWeights)), &Vertex::boneWeights, offsetof(Vertex, boneWeights), Span<const HypClassAttribute> { {HypClassAttribute("property", "BoneWeights"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(BoneIndices)), &Vertex::boneIndices, offsetof(Vertex, boneIndices), Span<const HypClassAttribute> { {HypClassAttribute("property", "BoneIndices"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(NumIndices)), &Vertex::numIndices, offsetof(Vertex, numIndices), Span<const HypClassAttribute> { {HypClassAttribute("property", "NumIndices"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(NumWeights)), &Vertex::numWeights, offsetof(Vertex, numWeights), Span<const HypClassAttribute> { {HypClassAttribute("property", "NumWeights"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion Vertex Reflection Data

static_assert(sizeof(Vertex) == 128, "Expected sizeof(Vertex) to be 128 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region VertexAttributeSet Reflection Data

HYP_BEGIN_STRUCT(VertexAttributeSet, 237, 0, {}, HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(FlagMask)), &VertexAttributeSet::flagMask, offsetof(VertexAttributeSet, flagMask))
HYP_END_STRUCT

#pragma endregion VertexAttributeSet Reflection Data

} // namespace hyperion

