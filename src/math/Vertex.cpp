/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Vertex.hpp>

#include <core/HypClassUtils.hpp>

namespace hyperion {

#pragma region VertexAttributeSet

HYP_DEFINE_CLASS(
    VertexAttributeSet,
    HypClassProperty(NAME("FlagMask"), &VertexAttributeSet::GetFlagMask, &VertexAttributeSet::SetFlagMask)
);

Array<VertexAttribute::Type> VertexAttributeSet::BuildAttributes() const
{
    Array<VertexAttribute::Type> attributes;
    attributes.Reserve(VertexAttribute::mapping.Size());

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
        const uint64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

        if (flag_mask & iter_flag_mask) {
            attributes.PushBack(VertexAttribute::Type(iter_flag_mask));
        }
    }

    return attributes;
}

SizeType VertexAttributeSet::CalculateVertexSize() const
{
    SizeType size = 0;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
        const uint64 iter_flag_mask = VertexAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

        if (flag_mask & iter_flag_mask) {
            size += VertexAttribute::mapping[VertexAttribute::Type(iter_flag_mask)].size;
        }
    }

    return size;
}

#pragma endregion VertexAttributeSet

#pragma region VertexAttribute

const decltype(VertexAttribute::mapping) VertexAttribute::mapping({
    { MESH_INPUT_ATTRIBUTE_POSITION,     VertexAttribute { .name = "a_position",     .location = 0, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_NORMAL,       VertexAttribute { .name = "a_normal",       .location = 1, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TEXCOORD0,    VertexAttribute { .name = "a_texcoord0",    .location = 2, .binding = 0, .size = 2 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TEXCOORD1,    VertexAttribute { .name = "a_texcoord1",    .location = 3, .binding = 0, .size = 2 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TANGENT,      VertexAttribute { .name = "a_tangent",      .location = 4, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BITANGENT,    VertexAttribute { .name = "a_bitangent",    .location = 5, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BONE_INDICES, VertexAttribute { .name = "a_bone_weights", .location = 6, .binding = 0, .size = 4 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS, VertexAttribute { .name = "a_bone_indices", .location = 7, .binding = 0, .size = 4 * sizeof(float) } }
});

#pragma endregion VertexAttribute

#pragma region Vertex

bool Vertex::operator==(const Vertex &other) const
{
    return position == other.position
        && normal == other.normal
        && texcoord0 == other.texcoord0
        && texcoord1 == other.texcoord1
        && tangent == other.tangent
        && num_indices == other.num_indices
        && num_weights == other.num_weights
        && bone_weights == other.bone_weights
        && bone_indices == other.bone_indices;
}

Vertex Vertex::operator*(float scalar) const
{
    Vertex other(*this);
    other.SetPosition(GetPosition() * scalar);

    return other;
}

Vertex &Vertex::operator*=(float scalar)
{
    return *this = operator*(scalar);
}

Vertex operator*(const Matrix4 &mat, const Vertex &vertex)
{
    Vertex other(vertex);
    other.SetPosition(mat * vertex.GetPosition());

    return other;
}

Vertex operator*(const Transform &transform, const Vertex &vertex)
{
    return transform.GetMatrix() * vertex;
}

#pragma endregion Vertex

} // namespace hyperion
