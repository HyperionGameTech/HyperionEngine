/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Vertex.hpp>

#ifndef HYP_BUILDTOOL
#include <Vertex.generated.inl>
#endif

namespace Hyperion {

#pragma region VertexAttributeSet

Array<VertexAttribute::Type> VertexAttributeSet::BuildAttributes() const
{
    Array<VertexAttribute::Type> attributes;
    attributes.Reserve(VertexAttribute::mapping.Size());

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const uint64 iterFlagMask = VertexAttribute::mapping.OrdinalToEnum(i); // NOLINT(readability-static-accessed-through-instance)

        if (flagMask & iterFlagMask)
        {
            attributes.PushBack(VertexAttribute::Type(iterFlagMask));
        }
    }

    return attributes;
}

SizeType VertexAttributeSet::CalculateVertexSize() const
{
    SizeType size = 0;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const uint64 iterFlagMask = VertexAttribute::mapping.OrdinalToEnum(i); // NOLINT(readability-static-accessed-through-instance)

        if (flagMask & iterFlagMask)
        {
            size += VertexAttribute::mapping[VertexAttribute::Type(iterFlagMask)].size;
        }
    }

    return size;
}

String VertexAttributeSet::ToString() const
{
    String result = "";
    bool first = true;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const uint64 iterFlagMask = VertexAttribute::mapping.OrdinalToEnum(i); // NOLINT(readability-static-accessed-through-instance)

        if (flagMask & iterFlagMask)
        {
            if (!first)
            {
                result += ", ";
            }

            result += VertexAttribute::mapping[VertexAttribute::Type(iterFlagMask)].name;

            first = false;
        }
    }

    result += "";

    return result;
}

#pragma endregion VertexAttributeSet

#pragma region VertexAttribute

const decltype(VertexAttribute::mapping) VertexAttribute::mapping = {
    { MESH_INPUT_ATTRIBUTE_POSITION, VertexAttribute { .name = "a_position", .location = 0, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_NORMAL, VertexAttribute { .name = "a_normal", .location = 1, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TEXCOORD0, VertexAttribute { .name = "a_texcoord0", .location = 2, .binding = 0, .size = 2 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TEXCOORD1, VertexAttribute { .name = "a_texcoord1", .location = 3, .binding = 0, .size = 2 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_TANGENT, VertexAttribute { .name = "a_tangent", .location = 4, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BITANGENT, VertexAttribute { .name = "a_bitangent", .location = 5, .binding = 0, .size = 3 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BONE_INDICES, VertexAttribute { .name = "a_bone_weights", .location = 6, .binding = 0, .size = 4 * sizeof(float) } },
    { MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS, VertexAttribute { .name = "a_bone_indices", .location = 7, .binding = 0, .size = 4 * sizeof(float) } }
};

#pragma endregion VertexAttribute

#pragma region Vertex

bool Vertex::operator==(const Vertex& other) const
{
    return position == other.position
        && normal == other.normal
        && texcoord0 == other.texcoord0
        && texcoord1 == other.texcoord1
        && tangent == other.tangent
        && numIndices == other.numIndices
        && numWeights == other.numWeights
        && boneWeights == other.boneWeights
        && boneIndices == other.boneIndices;
}

bool Vertex::operator!=(const Vertex& other) const
{
    return position != other.position
        || normal != other.normal
        || texcoord0 != other.texcoord0
        || texcoord1 != other.texcoord1
        || tangent != other.tangent
        || numIndices != other.numIndices
        || numWeights != other.numWeights
        || boneWeights != other.boneWeights
        || boneIndices != other.boneIndices;
}

#pragma endregion Vertex

} // namespace Hyperion
