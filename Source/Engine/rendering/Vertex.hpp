/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/math/Vector2.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Transform.hpp>

namespace Hyperion {

static constexpr uint32 MAX_BONE_INDICES = 4;
static constexpr uint32 MAX_BONE_WEIGHTS = 4;

struct VertexAttribute;

/*! \brief Represents a vertex in a mesh.
 *  \details This struct defines the properties of a vertex, including its position, normal, texture coordinates, tangent, bitangent,
 *  bone indices, and bone weights. It is used to represent the data for each vertex in a mesh and is essential for rendering operations.
 *  The struct is designed to be compatible with serialization and deserialization processes.
 */
HYP_STRUCT(Size = 128, Serialize = "bitwise")
struct alignas(16) Vertex
{
    HYP_STRUCT_BODY(Vertex);

    Vertex()
        : numIndices(0),
          numWeights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = 0;
            boneWeights[i] = 0;
        }
    }

    explicit Vertex(const Vector3& position)
        : position(position),
          numIndices(0),
          numWeights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = 0;
            boneWeights[i] = 0;
        }
    }

    explicit Vertex(const Vector3& position, const Vector2& texcoord0)
        : position(position),
          texcoord0(texcoord0),
          numIndices(0),
          numWeights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = 0;
            boneWeights[i] = 0;
        }
    }

    explicit Vertex(const Vector3& position, const Vector2& texcoord0, const Vector3& normal)
        : position(position),
          normal(normal),
          texcoord0(texcoord0),
          numIndices(0),
          numWeights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = 0;
            boneWeights[i] = 0;
        }
    }

    Vertex(const Vertex& other) = default;
    Vertex& operator=(const Vertex& other) = default;

    Vertex(Vertex&& other) noexcept = default;
    Vertex& operator=(Vertex&& other) noexcept = default;

    HYP_API bool operator==(const Vertex& other) const;
    HYP_API bool operator!=(const Vertex& other) const;

    HYP_API Vertex operator*(float scalar) const;
    HYP_API Vertex& operator*=(float scalar);

    HYP_FORCE_INLINE void SetPosition(const Vec3f& vec)
    {
        position = vec;
    }

    HYP_FORCE_INLINE const Vec3f& GetPosition() const
    {
        return position;
    }

    HYP_FORCE_INLINE void SetNormal(const Vec3f& vec)
    {
        normal = vec;
    }

    HYP_FORCE_INLINE const Vec3f& GetNormal() const
    {
        return normal;
    }

    HYP_FORCE_INLINE void SetTexCoord0(const Vec2f& vec)
    {
        texcoord0 = vec;
    }

    HYP_FORCE_INLINE const Vec2f& GetTexCoord0() const
    {
        return texcoord0;
    }

    HYP_FORCE_INLINE void SetTexCoord1(const Vec2f& vec)
    {
        texcoord1 = vec;
    }

    HYP_FORCE_INLINE const Vec2f& GetTexCoord1() const
    {
        return texcoord1;
    }

    HYP_FORCE_INLINE void SetTangent(const Vec3f& vec)
    {
        tangent = vec;
    }

    HYP_FORCE_INLINE const Vec3f& GetTangent() const
    {
        return tangent;
    }

    HYP_FORCE_INLINE void SetBitangent(const Vec3f& vec)
    {
        bitangent = vec;
    }

    HYP_FORCE_INLINE const Vec3f& GetBitangent() const
    {
        return bitangent;
    }

    HYP_FORCE_INLINE void AddBoneWeight(float val)
    {
        if (numWeights < MAX_BONE_WEIGHTS)
            boneWeights[numWeights++] = val;
    }

    HYP_FORCE_INLINE void SetBoneWeight(int i, float val)
    {
        boneWeights[i] = val;
    }

    HYP_FORCE_INLINE float GetBoneWeight(int i) const
    {
        return boneWeights[i];
    }

    HYP_FORCE_INLINE int GetNumWeights() const
    {
        return numWeights;
    }

    HYP_FORCE_INLINE const FixedArray<float, MAX_BONE_WEIGHTS>& GetBoneWeights() const
    {
        return boneWeights;
    }

    HYP_FORCE_INLINE void SetBoneWeights(const FixedArray<float, MAX_BONE_WEIGHTS>& weights)
    {
        numWeights = 0;

        for (int i = 0; i < MAX_BONE_WEIGHTS; i++)
        {
            if (weights[i] != 0.0f)
            {
                boneWeights[i] = weights[i];
                numWeights = i + 1;
            }
        }
    }

    HYP_FORCE_INLINE void AddBoneIndex(int val)
    {
        if (numIndices < MAX_BONE_INDICES)
            boneIndices[numIndices++] = val;
    }

    HYP_FORCE_INLINE void SetBoneIndex(int i, int val)
    {
        boneIndices[i] = val;
    }

    HYP_FORCE_INLINE int GetBoneIndex(int i) const
    {
        return boneIndices[i];
    }

    HYP_FORCE_INLINE int GetNumIndices() const
    {
        return numIndices;
    }

    HYP_FORCE_INLINE const FixedArray<uint32, MAX_BONE_INDICES>& GetBoneIndices() const
    {
        return boneIndices;
    }

    HYP_FORCE_INLINE void SetBoneIndices(const FixedArray<uint32, MAX_BONE_INDICES>& indices)
    {
        numIndices = 0;

        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            if (indices[i] != 0)
            {
                boneIndices[i] = indices[i];
                numIndices = i + 1;
            }
        }
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode()
            .Combine(position)
            .Combine(normal)
            .Combine(texcoord0)
            .Combine(texcoord1)
            .Combine(tangent)
            .Combine(bitangent)
            .Combine(numIndices)
            .Combine(numWeights)
            .Combine(boneIndices)
            .Combine(boneWeights);
    }

    HYP_FIELD(Property = "Position", Serialize = true)
    Vec3f position;

    HYP_FIELD(Property = "Normal", Serialize = true)
    Vec3f normal;

    HYP_FIELD(Property = "Tangent", Serialize = true)
    Vec3f tangent;

    HYP_FIELD(Property = "Bitangent", Serialize = true)
    Vec3f bitangent;

    HYP_FIELD(Property = "TexCoord0", Serialize = true)
    Vec2f texcoord0;

    HYP_FIELD(Property = "TexCoord1", Serialize = true)
    Vec2f texcoord1;

    HYP_FIELD(Property = "BoneWeights", Serialize = true)
    FixedArray<float, MAX_BONE_WEIGHTS> boneWeights;

    HYP_FIELD(Property = "BoneIndices", Serialize = true)
    FixedArray<uint32, MAX_BONE_INDICES> boneIndices;

    HYP_FIELD(Property = "NumIndices", Serialize = true)
    uint8 numIndices;

    HYP_FIELD(Property = "NumWeights", Serialize = true)
    uint8 numWeights;
};

static_assert(alignof(Vertex) == 16, "Vertex alignment is not 16 bytes, ensure size matches C# Vertex struct alignment");

} // namespace Hyperion

HYP_DEF_STL_HASH(Hyperion::Vertex);