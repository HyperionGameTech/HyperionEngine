#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Vertex Reflection Data

HYP_BEGIN_STRUCT(Vertex, 250, 0, {}, ClassAttribute("size", 128),ClassAttribute("serialize", "bitwise"))
    Field(NAME(HYP_STR(Position)), &Vertex::position, offsetof(Vertex, position), Span<const ClassAttribute> { {ClassAttribute("property", "Position"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Normal)), &Vertex::normal, offsetof(Vertex, normal), Span<const ClassAttribute> { {ClassAttribute("property", "Normal"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Tangent)), &Vertex::tangent, offsetof(Vertex, tangent), Span<const ClassAttribute> { {ClassAttribute("property", "Tangent"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Bitangent)), &Vertex::bitangent, offsetof(Vertex, bitangent), Span<const ClassAttribute> { {ClassAttribute("property", "Bitangent"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Texcoord0)), &Vertex::texcoord0, offsetof(Vertex, texcoord0), Span<const ClassAttribute> { {ClassAttribute("property", "TexCoord0"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Texcoord1)), &Vertex::texcoord1, offsetof(Vertex, texcoord1), Span<const ClassAttribute> { {ClassAttribute("property", "TexCoord1"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(BoneWeights)), &Vertex::boneWeights, offsetof(Vertex, boneWeights), Span<const ClassAttribute> { {ClassAttribute("property", "BoneWeights"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(BoneIndices)), &Vertex::boneIndices, offsetof(Vertex, boneIndices), Span<const ClassAttribute> { {ClassAttribute("property", "BoneIndices"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(NumIndices)), &Vertex::numIndices, offsetof(Vertex, numIndices), Span<const ClassAttribute> { {ClassAttribute("property", "NumIndices"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(NumWeights)), &Vertex::numWeights, offsetof(Vertex, numWeights), Span<const ClassAttribute> { {ClassAttribute("property", "NumWeights"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion Vertex Reflection Data

static_assert(sizeof(Vertex) == 128, "Expected sizeof(Vertex) to be 128 bytes");
} // namespace hyperion


namespace hyperion {

#pragma region VertexAttributeSet Reflection Data

HYP_BEGIN_STRUCT(VertexAttributeSet, 251, 0, {}, ClassAttribute("serialize", "bitwise"))
    Field(NAME(HYP_STR(FlagMask)), &VertexAttributeSet::flagMask, offsetof(VertexAttributeSet, flagMask))
HYP_END_STRUCT

#pragma endregion VertexAttributeSet Reflection Data

} // namespace hyperion

