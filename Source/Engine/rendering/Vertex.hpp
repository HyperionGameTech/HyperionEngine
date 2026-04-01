/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

HYP_ENUM()
enum VertexType : uint8
{
    VT_Position = 0x1,
    VT_Normal = 0x2,
    VT_UV0 = 0x4,
    VT_UV1 = 0x8,
    VT_Simple = VT_Position | VT_Normal | VT_UV0,
    VT_Skeletal = 0x10
};

#pragma pack(push, 1)

template <uint8> struct TVertexPacket;

template <> struct TVertexPacket<VT_Position>
{
    float posX;
    float posY;
    float posZ;
};

template <> struct TVertexPacket<VT_Normal>
{
    float normalX;
    float normalY;
    float normalZ;
};

template <> struct TVertexPacket<VT_UV0>
{
    float uv0[2];
};

template <> struct TVertexPacket<VT_UV1>
{
    float uv1[2];
};

template <> struct TVertexPacket<VT_Skeletal>
{
    static constexpr uint8 InvalidBoneIndex = UINT8_MAX;

    float boneWeights[4] {};
    uint32 boneIndices = UINT32_MAX;
    
    HYP_FORCE_INLINE void SetBoneWeight(int i, float val)
    {
        boneWeights[i] = val;
    }

    HYP_FORCE_INLINE float GetBoneWeight(int i) const
    {
        return boneWeights[i];
    }

    HYP_FORCE_INLINE int NumBoneWeights() const
    {
        return NumBoneIndices();
    }

    HYP_FORCE_INLINE const float* GetBoneWeights() const
    {
        return boneWeights;
    }

    HYP_FORCE_INLINE void SetBoneWeights(const float (&weights)[4])
    {
        Memory::Copy(boneWeights, weights, sizeof(boneWeights));
    }

    void AddBoneWeight(float boneWeight)
    {
        const uint32 numWeights = NumBoneWeights();

        if (numWeights < MAX_BONE_WEIGHTS)
        {
            boneWeights[numWeights] = boneWeight;
        }
    }

    HYP_FORCE_INLINE void SetBoneIndex(int i, uint8 boneIndex)
    {
        uint8* boneIndicesU8 = reinterpret_cast<uint8*>(&boneIndices);
        boneIndicesU8[i] = boneIndex;
    }

    HYP_FORCE_INLINE uint8 GetBoneIndex(int i) const
    {
        const uint8* boneIndicesU8 = reinterpret_cast<const uint8*>(&boneIndices);
        return boneIndicesU8[i];
    }

    HYP_FORCE_INLINE uint32 NumBoneIndices() const
    {
        uint32 count = 0;
        
        const uint8* boneIndicesU8 = reinterpret_cast<const uint8*>(&boneIndices);

        for (uint32 i = 0; i < MAX_BONE_INDICES; i++)
        {
            if (boneIndicesU8[i] == InvalidBoneIndex)
                break;

            count++;
        }

        return count;
    }

    HYP_FORCE_INLINE const uint8* GetBoneIndices() const
    {
        const uint8* boneIndicesU8 = reinterpret_cast<const uint8*>(&boneIndices);
        return boneIndicesU8;
    }

    HYP_FORCE_INLINE void SetBoneIndices(const uint8 (&indices)[4])
    {
        boneIndices = *reinterpret_cast<const uint32*>(indices);
    }

    void AddBoneIndex(uint8 boneIndex)
    {
        const uint32 numIndices = NumBoneIndices();
        
        uint8* boneIndicesU8 = reinterpret_cast<uint8*>(&boneIndices);

        if (numIndices < MAX_BONE_INDICES)
        {
            boneIndicesU8[numIndices] = boneIndex;
        }
    }
};

namespace detail {

template <uint8 Bit>
struct TVertexPacketEmpty {};

template <uint8 PacketMask, uint8 Bit>
using TVertexPacketConditional = std::conditional_t<(PacketMask & Bit) != 0, TVertexPacket<Bit>, TVertexPacketEmpty<Bit>>;

template <uint8 PacketMask, uint8... Bits>
struct TVertexPacketExpander : TVertexPacketConditional<PacketMask, Bits>...
{
};

} // namespace detail

template <uint8 PacketMask>
struct TVertex
    : detail::TVertexPacketExpander<PacketMask,
        VT_Position,
        VT_Normal,
        VT_UV0,
        VT_UV1,
        VT_Skeletal>
{
};

using SimpleVertex = TVertex<VT_Simple>;
using SkeletalVertex = TVertex<VT_Simple | VT_Skeletal>;

HYP_STRUCT(Serialize = "bitwise")
struct VertexInputLayoutDesc
{
    HYP_STRUCT_BODY(VertexInputLayoutDesc);

    uint8 packetMask;
    uint8 vertexSize;

    constexpr bool operator==(const VertexInputLayoutDesc& other) const = default;
    constexpr bool operator!=(const VertexInputLayoutDesc& other) const = default;

    constexpr HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(packetMask)
            .Combine(vertexSize);
    }
};

template <uint8 PacketMask>
static inline constexpr VertexInputLayoutDesc StaticVertexInputLayout = VertexInputLayoutDesc { PacketMask, sizeof(TVertex<PacketMask>) };

#pragma pack(pop)

struct VertexArrayView
{
    const float* floatData;
    size_t vertexCount;

    VertexInputLayoutDesc layoutDesc;
    
    VertexArrayView() = default;

    template <uint8 PacketMask>
    VertexArrayView(const TVertex<PacketMask>* vertexData, size_t vertexCount)
        : floatData(reinterpret_cast<const float*>(vertexData)),
          vertexCount(vertexCount),
          layoutDesc { uint8(sizeof(TVertex<PacketMask>)), PacketMask }
    {
        static_assert(alignof(TVertex<PacketMask>) == 16);
        static_assert(sizeof(TVertex<PacketMask>) <= UINT8_MAX);
    }
};

/*! \brief Represents a vertex in a mesh.
 *  \details This struct defines the properties of a vertex, including its position, normal, texture coordinates, tangent, bitangent,
 *  bone indices, and bone weights. It is used to represent the data for each vertex in a mesh and is essential for rendering operations.
 *  The struct is designed to be compatible with serialization and deserialization processes.
 */
HYP_STRUCT()
struct alignas(16) Vertex
{
    HYP_STRUCT_BODY(Vertex);

    static constexpr uint8 InvalidBoneIndex = UINT8_MAX;

    Vertex()
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = InvalidBoneIndex;
            boneWeights[i] = 0.0f;
        }
    }

    explicit Vertex(const Vec3f& position)
        : position(position)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = InvalidBoneIndex;
            boneWeights[i] = 0.0f;
        }
    }

    explicit Vertex(const Vec3f& position, const Vec2f& texcoord0)
        : position(position),
          texcoord0(texcoord0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = InvalidBoneIndex;
            boneWeights[i] = 0.0f;
        }
    }

    explicit Vertex(const Vec3f& position, const Vec2f& texcoord0, const Vec3f& normal)
        : position(position),
          normal(normal),
          texcoord0(texcoord0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++)
        {
            boneIndices[i] = InvalidBoneIndex;
            boneWeights[i] = 0.0f;
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

    HYP_FORCE_INLINE void SetBoneWeight(int i, float val)
    {
        boneWeights[i] = val;
    }

    HYP_FORCE_INLINE float GetBoneWeight(int i) const
    {
        return boneWeights[i];
    }

    HYP_FORCE_INLINE int NumBoneWeights() const
    {
        return NumBoneIndices();
    }

    HYP_FORCE_INLINE const FixedArray<float, MAX_BONE_WEIGHTS>& GetBoneWeights() const
    {
        return boneWeights;
    }

    HYP_FORCE_INLINE void SetBoneWeights(const FixedArray<float, MAX_BONE_WEIGHTS>& weights)
    {
        boneWeights = weights;
    }

    void AddBoneWeight(float boneWeight)
    {
        const uint32 numWeights = NumBoneWeights();

        if (numWeights < MAX_BONE_WEIGHTS)
        {
            boneWeights[numWeights] = boneWeight;
        }
    }

    HYP_FORCE_INLINE void SetBoneIndex(int i, uint8 boneIndex)
    {
        boneIndices[i] = boneIndex;
    }

    HYP_FORCE_INLINE uint8 GetBoneIndex(int i) const
    {
        return boneIndices[i];
    }

    HYP_FORCE_INLINE constexpr uint32 NumBoneIndices() const
    {
        uint32 count = 0;

        for (uint32 i = 0; i < MAX_BONE_INDICES; i++)
        {
            if (boneIndices[i] == InvalidBoneIndex)
                break;

            count++;
        }

        return count;
    }

    HYP_FORCE_INLINE const FixedArray<uint8, MAX_BONE_INDICES>& GetBoneIndices() const
    {
        return boneIndices;
    }

    HYP_FORCE_INLINE void SetBoneIndices(const FixedArray<uint8, MAX_BONE_INDICES>& indices)
    {
        boneIndices = indices;
    }

    void AddBoneIndex(uint8 boneIndex)
    {
        const uint32 numIndices = NumBoneIndices();

        if (numIndices < MAX_BONE_INDICES)
        {
            boneIndices[numIndices] = boneIndex;
        }
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        const uint32 numBoneIndices = NumBoneIndices();

        return HashCode()
            .Combine(position)
            .Combine(normal)
            .Combine(texcoord0)
            .Combine(texcoord1)
            .Combine(tangent)
            .Combine(bitangent)
            .Combine(HashCode::GetHashCode(boneIndices.Begin(), boneIndices.Begin() + numBoneIndices))
            .Combine(HashCode::GetHashCode(boneWeights.Begin(), boneWeights.Begin() + numBoneIndices));
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
    FixedArray<uint8, MAX_BONE_INDICES> boneIndices;
};

static_assert(alignof(Vertex) == 16, "Vertex alignment is not 16 bytes, ensure size matches C# Vertex struct alignment");

} // namespace Hyperion

HYP_DEF_STL_HASH(::Hyperion::Vertex);