/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Vertex.hpp>

#ifndef HYP_BUILDTOOL
#include <Vertex.generated.inl>
#endif

namespace Hyperion {

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
