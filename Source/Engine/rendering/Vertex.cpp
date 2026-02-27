/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <rendering/Vertex.hpp>

#ifndef HYP_TOOL
#include <Vertex.generated.inl>
#endif

namespace Hyperion {

#pragma region Vertex

bool Vertex::operator==(const Vertex& other) const
{
    const uint32 numBoneIndices = NumBoneIndices();

    if (numBoneIndices != other.NumBoneIndices())
        return false;

    return position == other.position
        && normal == other.normal
        && texcoord0 == other.texcoord0
        && texcoord1 == other.texcoord1
        && tangent == other.tangent
        && Memory::Compare(&boneIndices[0], &other.boneIndices[0], numBoneIndices * sizeof(uint32)) == 0
        && Memory::Compare(reinterpret_cast<const uint32*>(&boneWeights[0]), reinterpret_cast<const uint32*>(&other.boneWeights[0]), numBoneIndices * sizeof(float)) == 0;
}

bool Vertex::operator!=(const Vertex& other) const
{
    return !operator==(other);
}

#pragma endregion Vertex

} // namespace Hyperion