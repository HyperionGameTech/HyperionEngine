/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VERTEX_HPP
#define HYPERION_VERTEX_HPP

#define MAX_BONE_WEIGHTS 4
#define MAX_BONE_INDICES 4

#include <core/containers/FixedArray.hpp>

#include <core/utilities/Variant.hpp>

#include <core/Defines.hpp>

#include <util/EnumOptions.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Transform.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

struct VertexAttribute
{
    enum Type : uint64
    {
        MESH_INPUT_ATTRIBUTE_UNDEFINED      = 0x0,
        MESH_INPUT_ATTRIBUTE_POSITION       = 0x1,
        MESH_INPUT_ATTRIBUTE_NORMAL         = 0x2,
        MESH_INPUT_ATTRIBUTE_TEXCOORD0      = 0x4,
        MESH_INPUT_ATTRIBUTE_TEXCOORD1      = 0x8,
        MESH_INPUT_ATTRIBUTE_TANGENT        = 0x10,
        MESH_INPUT_ATTRIBUTE_BITANGENT      = 0x20,
        MESH_INPUT_ATTRIBUTE_BONE_INDICES   = 0x40,
        MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS   = 0x80,
    };

    static const EnumOptions<Type, VertexAttribute, 16> mapping;

    const char  *name;
    uint32      location;
    uint32      binding;
    uint32      size; // total size == num elements * 4

    HYP_FORCE_INLINE bool operator<(const VertexAttribute &other) const
        { return location < other.location; }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(location);
        hc.Add(binding);
        hc.Add(size);

        return hc;
    }
};

HYP_STRUCT()
struct VertexAttributeSet
{
    HYP_FIELD(Property="FlagMask", Serialize=true)
    uint64 flag_mask;

    constexpr VertexAttributeSet()
        : flag_mask(0) {}

    constexpr VertexAttributeSet(uint64 flag_mask)
        : flag_mask(flag_mask) {}

    constexpr VertexAttributeSet(VertexAttribute::Type flags)
        : flag_mask(uint64(flags)) {}

    constexpr VertexAttributeSet(const VertexAttributeSet &other)   = default;
    VertexAttributeSet &operator=(const VertexAttributeSet &other)  = default;
    ~VertexAttributeSet()                                           = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return flag_mask != 0; }

    HYP_FORCE_INLINE bool operator==(const VertexAttributeSet &other) const
        { return flag_mask == other.flag_mask; }

    HYP_FORCE_INLINE bool operator!=(const VertexAttributeSet &other) const
        { return flag_mask != other.flag_mask; }

    HYP_FORCE_INLINE bool operator==(uint64 flags) const
        { return flag_mask == flags; }

    HYP_FORCE_INLINE bool operator!=(uint64 flags) const
        { return flag_mask != flags; }

    HYP_FORCE_INLINE VertexAttributeSet operator~() const
        { return ~flag_mask; }

    HYP_FORCE_INLINE VertexAttributeSet operator&(const VertexAttributeSet &other) const
        { return { flag_mask & other.flag_mask }; }

    HYP_FORCE_INLINE VertexAttributeSet &operator&=(const VertexAttributeSet &other)
        { flag_mask &= other.flag_mask; return *this; }

    HYP_FORCE_INLINE VertexAttributeSet operator&(uint64 flags) const
        { return { flag_mask & flags }; }

    HYP_FORCE_INLINE VertexAttributeSet &operator&=(uint64 flags)
        { flag_mask &= flags; return *this; }

    HYP_FORCE_INLINE VertexAttributeSet operator|(const VertexAttributeSet &other) const
        { return { flag_mask | other.flag_mask }; }

    HYP_FORCE_INLINE VertexAttributeSet &operator|=(const VertexAttributeSet &other)
        { flag_mask |= other.flag_mask; return *this; }

    HYP_FORCE_INLINE VertexAttributeSet operator|(uint64 flags) const
        { return { flag_mask | flags }; }

    HYP_FORCE_INLINE VertexAttributeSet &operator|=(uint64 flags)
        { flag_mask |= flags; return *this; }

    HYP_FORCE_INLINE bool operator<(const VertexAttributeSet &other) const
        { return flag_mask < other.flag_mask; }

    HYP_FORCE_INLINE bool Has(VertexAttribute::Type type) const
        { return bool(operator&(uint64(type))); }

    HYP_FORCE_INLINE void Set(uint64 flags, bool enable = true)
    {
        if (enable) {
            flag_mask |= flags;
        } else {
            flag_mask &= ~flags;
        }
    }

    HYP_FORCE_INLINE void Set(VertexAttribute::Type type, bool enable = true)
        { Set(uint64(type), enable); }

    HYP_FORCE_INLINE void Merge(const VertexAttributeSet &other)
        { flag_mask |= other.flag_mask; }

    HYP_FORCE_INLINE uint64 GetFlagMask() const
        { return flag_mask; }

    HYP_FORCE_INLINE void SetFlagMask(uint64 flags)
        { flag_mask = flags; }

    HYP_FORCE_INLINE uint32 Size() const
        { return uint32(ByteUtil::BitCount(flag_mask)); }

    HYP_API Array<VertexAttribute::Type> BuildAttributes() const;
    HYP_API SizeType CalculateVertexSize() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flag_mask);

        return hc;
    }
};

constexpr VertexAttributeSet static_mesh_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT
);

constexpr VertexAttributeSet skeleton_vertex_attributes(
    VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS
    | VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES
);

HYP_STRUCT(Size=128, Serialize="bitwise")
struct alignas(16) Vertex
{
    friend Vertex operator*(const Matrix4 &mat, const Vertex &vertex);
    friend Vertex operator*(const Transform &transform, const Vertex &vertex);

    Vertex()
        : num_indices(0),
          num_weights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    explicit Vertex(const Vector3 &position)
        : position(position),
          num_indices(0),
          num_weights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    explicit Vertex(const Vector3 &position, const Vector2 &texcoord0)
        : position(position),
          texcoord0(texcoord0),
          num_indices(0),
          num_weights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    explicit Vertex(const Vector3 &position, const Vector2 &texcoord0, const Vector3 &normal)
        : position(position),
          normal(normal),
          texcoord0(texcoord0),
          num_indices(0),
          num_weights(0)
    {
        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            bone_indices[i] = 0;
            bone_weights[i] = 0;
        }
    }

    Vertex(const Vertex &other)                 = default;
    Vertex &operator=(const Vertex &other)      = default;

    Vertex(Vertex &&other) noexcept             = default;
    Vertex &operator=(Vertex &&other) noexcept  = default;

    bool operator==(const Vertex &other) const;
    // Vertex &operator=(const Vertex &other);
    Vertex operator*(float scalar) const;
    Vertex &operator*=(float scalar);

    HYP_FORCE_INLINE void SetPosition(const Vec3f &vec)
        { position = vec; }

    HYP_FORCE_INLINE const Vec3f &GetPosition() const
        { return position; }

    HYP_FORCE_INLINE void SetNormal(const Vec3f &vec)
        { normal = vec; }

    HYP_FORCE_INLINE const Vec3f &GetNormal() const
        { return normal; }

    HYP_FORCE_INLINE void SetTexCoord0(const Vec2f &vec)
        { texcoord0 = vec; }

    HYP_FORCE_INLINE const Vec2f &GetTexCoord0() const
        { return texcoord0; }

    HYP_FORCE_INLINE void SetTexCoord1(const Vec2f &vec)
        { texcoord1 = vec; }

    HYP_FORCE_INLINE const Vec2f &GetTexCoord1() const
        { return texcoord1; }

    HYP_FORCE_INLINE void SetTangent(const Vec3f &vec)
        { tangent = vec; }

    HYP_FORCE_INLINE const Vec3f &GetTangent() const
        { return tangent; }

    HYP_FORCE_INLINE void SetBitangent(const Vec3f &vec)
        { bitangent = vec; }

    HYP_FORCE_INLINE const Vec3f &GetBitangent() const
        { return bitangent; }

    HYP_FORCE_INLINE void AddBoneWeight(float val)
        { if (num_weights < MAX_BONE_WEIGHTS) bone_weights[num_weights++] = val; }

    HYP_FORCE_INLINE void SetBoneWeight(int i, float val)
        { bone_weights[i] = val; }

    HYP_FORCE_INLINE float GetBoneWeight(int i) const
        { return bone_weights[i]; }

    HYP_FORCE_INLINE int GetNumWeights() const
        { return num_weights; }

    HYP_FORCE_INLINE const FixedArray<float, MAX_BONE_WEIGHTS> &GetBoneWeights() const
        { return bone_weights; }

    HYP_FORCE_INLINE void SetBoneWeights(const FixedArray<float, MAX_BONE_WEIGHTS> &weights)
    {
        num_weights = 0;

        for (int i = 0; i < MAX_BONE_WEIGHTS; i++) {
            if (weights[i] != 0.0f) {
                bone_weights[i] = weights[i];
                num_weights = i + 1;
            }
        }
    }

    HYP_FORCE_INLINE void AddBoneIndex(int val)
        { if (num_indices < MAX_BONE_INDICES) bone_indices[num_indices++] = val; }

    HYP_FORCE_INLINE void SetBoneIndex(int i, int val)
        { bone_indices[i] = val; }

    HYP_FORCE_INLINE int GetBoneIndex(int i) const
        { return bone_indices[i]; }

    HYP_FORCE_INLINE int GetNumIndices() const
        { return num_indices; }

    HYP_FORCE_INLINE const FixedArray<uint32, MAX_BONE_INDICES> &GetBoneIndices() const
        { return bone_indices; }

    HYP_FORCE_INLINE void SetBoneIndices(const FixedArray<uint32, MAX_BONE_INDICES> &indices)
    {
        num_indices = 0;

        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            if (indices[i] != 0) {
                bone_indices[i] = indices[i];
                num_indices = i + 1;
            }
        }
    }

    /*! \brief Read the attribute from the vertex into \ref{ptr}. The value at \ref{ptr} must be able to hold sizeof(float) * 4.
     *  If an invalid attribute is passed, the function does nothing.
     *
     *  \param attr The attribute to read.
     *  \param ptr The pointer to write the attribute to.
     */
    void ReadAttribute(VertexAttribute::Type attr, void *ptr) const
    {
        switch (attr) {
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION:
            Memory::MemCpy(ptr, &position, sizeof(float) * 3);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL:
            Memory::MemCpy(ptr, &normal, sizeof(float) * 3);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT:
            Memory::MemCpy(ptr, &tangent, sizeof(float) * 3);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT:
            Memory::MemCpy(ptr, &bitangent, sizeof(float) * 3);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0:
            Memory::MemCpy(ptr, &texcoord0, sizeof(float) * 2);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1:
            Memory::MemCpy(ptr, &texcoord1, sizeof(float) * 2);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES:
            Memory::MemCpy(ptr, bone_indices.Data(), sizeof(uint32) * MAX_BONE_INDICES);
            break;
        case VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS:
            Memory::MemCpy(ptr, bone_weights.Data(), sizeof(float) * MAX_BONE_WEIGHTS);
            break;
        default:
            // Do nothing
            break;
        }
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(position.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(texcoord0.GetHashCode());
        hc.Add(texcoord1.GetHashCode());
        hc.Add(tangent.GetHashCode());
        hc.Add(bitangent.GetHashCode());
        hc.Add(num_indices);
        hc.Add(num_weights);

        for (int i = 0; i < MAX_BONE_INDICES; i++) {
            hc.Add(bone_indices[i]);
        }

        for (int i = 0; i < MAX_BONE_WEIGHTS; i++) {
            hc.Add(bone_weights[i]);
        }

        return hc;
    }

    HYP_FIELD(Property="Position", Serialize=true)
    Vec3f                               position;

    HYP_FIELD(Property="Normal", Serialize=true)
    Vec3f                               normal;

    HYP_FIELD(Property="Tangent", Serialize=true)
    Vec3f                               tangent;

    HYP_FIELD(Property="Bitangent", Serialize=true)
    Vec3f                               bitangent;

    HYP_FIELD(Property="TexCoord0", Serialize=true)
    Vec2f                               texcoord0;

    HYP_FIELD(Property="TexCoord1", Serialize=true)
    Vec2f                               texcoord1;

    HYP_FIELD(Property="BoneWeights", Serialize=true)
    FixedArray<float, MAX_BONE_WEIGHTS> bone_weights;

    HYP_FIELD(Property="BoneIndices", Serialize=true)
    FixedArray<uint32, MAX_BONE_INDICES>  bone_indices;

    HYP_FIELD(Property="NumIndices", Serialize=true)
    uint8                               num_indices;

    HYP_FIELD(Property="NumWeights", Serialize=true)
    uint8                               num_weights;
};

Vertex operator*(const Matrix4 &mat, const Vertex &vertex);
Vertex operator*(const Transform &transform, const Vertex &vertex);

static_assert(alignof(Vertex) == 16, "Vertex alignment is not 16 bytes, ensure size matches C# Vertex struct alignment");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vertex);

#endif
