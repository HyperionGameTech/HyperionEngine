/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Transform.hpp>

namespace Hyperion {

static constexpr uint32 MAX_BONE_INDICES = 4;
static constexpr uint32 MAX_BONE_WEIGHTS = 4;

struct VertexAttribute;

HYP_ENUM()
enum VertexType : uint8
{
    VT_Invalid = 0x0,

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

    TVertexPacket() = default;

    TVertexPacket(float posX, float posY, float posZ)
        : posX(posX),
          posY(posY),
          posZ(posZ)
    {
    }

    TVertexPacket(const Vec3f& position)
    {
        SetPosition(position);
    }

    HYP_FORCE_INLINE Vec3f GetPosition() const
    {
        return { posX, posY, posZ };
    }

    HYP_FORCE_INLINE void SetPosition(const Vec3f& position)
    {
        posX = position.x;
        posY = position.y;
        posZ = position.z;
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(posX)
            .Combine(HashCode::GetHashCode(posY))
            .Combine(HashCode::GetHashCode(posZ));
    }
};

template <> struct TVertexPacket<VT_Normal>
{
    float normalX;
    float normalY;
    float normalZ;

    TVertexPacket() = default;

    TVertexPacket(float normalX, float normalY, float normalZ)
        : normalX(normalX),
          normalY(normalY),
          normalZ(normalZ)
    {
    }

    TVertexPacket(const Vec3f& normal)
    {
        SetNormal(normal);
    }

    HYP_FORCE_INLINE Vec3f GetNormal() const
    {
        return { normalX, normalY, normalZ };
    }

    HYP_FORCE_INLINE void SetNormal(const Vec3f& normal)
    {
        normalX = normal.x;
        normalY = normal.y;
        normalZ = normal.z;
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(normalX)
            .Combine(HashCode::GetHashCode(normalY))
            .Combine(HashCode::GetHashCode(normalZ));
    }
};

template <> struct TVertexPacket<VT_UV0>
{
    float uv0[2];

    TVertexPacket() = default;

    TVertexPacket(float uv0_s, float uv0_t)
        : uv0 { uv0_s, uv0_t }
    {
    }

    TVertexPacket(const Vec2f& uv0)
    {
        SetUV0(uv0);
    }

    HYP_FORCE_INLINE Vec2f GetUV0() const
    {
        return { uv0[0], uv0[1] };
    }

    HYP_FORCE_INLINE void SetUV0(const Vec2f& uv0)
    {
        this->uv0[0] = uv0.x;
        this->uv0[1] = uv0.y;
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(uv0);
    }
};

template <> struct TVertexPacket<VT_UV1>
{
    float uv1[2];

    TVertexPacket() = default;

    TVertexPacket(float uv1_s, float uv1_t)
        : uv1 { uv1_s, uv1_t }
    {
    }

    TVertexPacket(const Vec2f& uv1)
    {
        SetUV1(uv1);
    }

    HYP_FORCE_INLINE Vec2f GetUV1() const
    {
        return { uv1[0], uv1[1] };
    }

    HYP_FORCE_INLINE void SetUV1(const Vec2f& uv1)
    {
        this->uv1[0] = uv1.x;
        this->uv1[1] = uv1.y;
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(uv1);
    }
};

template <> struct TVertexPacket<VT_Skeletal>
{
    static constexpr uint8 InvalidBoneIndex = UINT8_MAX;

    uint32 boneIndices = UINT32_MAX;
    float boneWeights[4] {};

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

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(boneIndices)
            .Combine(HashCode::GetHashCode(boneWeights));
    }
};

namespace detail {

template <uint8... TBits>
struct TVertexPacketBase : TVertexPacket<TBits>...
{
    constexpr HashCode GetHashCode() const
    {
        HashCode hc;

        ((hc = hc.Combine(TVertexPacket<TBits>::GetHashCode())), ...);

        return hc;
    }
};

template <uint8 TMask, typename TActiveSeq, uint8... TRemaining>
struct TVertexPacketFilterImpl;

template <uint8 TMask, uint8... TActive>
struct TVertexPacketFilterImpl<TMask, std::integer_sequence<uint8, TActive...>>
{
    using type = TVertexPacketBase<TActive...>;
};

template <uint8 TMask, uint8... TActive, uint8 THead, uint8... TTail>
struct TVertexPacketFilterImpl<TMask, std::integer_sequence<uint8, TActive...>, THead, TTail...>
{
    using type = typename TVertexPacketFilterImpl<
        TMask,
        std::conditional_t<
            (TMask & THead) != 0,
            std::integer_sequence<uint8, TActive..., THead>,
            std::integer_sequence<uint8, TActive...>
        >,
        TTail...
    >::type;
};

template <uint8 TMask, uint8... TAllBits>
using TVertexPacketExpander = typename TVertexPacketFilterImpl<TMask, std::integer_sequence<uint8>, TAllBits...>::type;

} // namespace detail

template <uint8 TMask>
struct TVertex
    : detail::TVertexPacketExpander<
        TMask,
        VT_Position,
        VT_Normal,
        VT_UV0,
        VT_UV1,
        VT_Skeletal>
{
    HYP_FORCE_INLINE bool operator==(const TVertex& other) const
    {
        return std::memcmp(this, &other, sizeof(TVertex)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const TVertex& other) const
    {
        return std::memcmp(this, &other, sizeof(TVertex)) != 0;
    }
};

using SimpleVertex = TVertex<VT_Simple>;
using SkeletalVertex = TVertex<VT_Simple | VT_Skeletal>;

HYP_STRUCT(Serialize = "bitwise")
struct VertexInputLayoutDesc
{
    HYP_STRUCT_BODY(VertexInputLayoutDesc);

    HYP_FIELD()
    uint8 mask;

    size_t VertexSize() const;

    constexpr bool operator==(const VertexInputLayoutDesc& other) const = default;
    constexpr bool operator!=(const VertexInputLayoutDesc& other) const = default;

    constexpr HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(mask);
    }

    String GetDebugString() const;
};

template <uint8 PacketMask>
static inline constexpr VertexInputLayoutDesc StaticVertexInputLayout = VertexInputLayoutDesc { PacketMask };

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
          layoutDesc { PacketMask }
    {
        static_assert(alignof(TVertex<PacketMask>) <= 16);
        static_assert(sizeof(TVertex<PacketMask>) <= UINT8_MAX);
    }
};

namespace VertexUtils {

static inline const char* ToString(VertexType vt)
{
    switch (vt)
    {
    case VertexType::VT_Position:
        return "Position";
    case VertexType::VT_Normal:
        return "Normal";
    case VertexType::VT_UV0:
        return "UV0";
    case VertexType::VT_UV1:
        return "UV1";
    case VertexType::VT_Skeletal:
        return "Skeletal";
    }

    return "";
}

static constexpr inline size_t PacketSize(VertexType vt)
{
    //AssertDebug(ByteUtil::BitCount(vt) == 1);

    switch (vt)
    {
    case VertexType::VT_Position:
        return sizeof(TVertexPacket<VT_Position>);
    case VertexType::VT_Normal:
        return sizeof(TVertexPacket<VT_Normal>);
    case VertexType::VT_UV0:
        return sizeof(TVertexPacket<VT_UV0>);
    case VertexType::VT_UV1:
        return sizeof(TVertexPacket<VT_UV1>);
    case VertexType::VT_Skeletal:
        return sizeof(TVertexPacket<VT_Skeletal>);
    default:
        //HYP_UNREACHABLE();
        return 0;
    }
}

namespace detail {


template <uint8 TMask, uint8 TBit>
static constexpr size_t TVertexSizeCalcConditional = (((TMask & TBit) != 0) ? PacketSize(VertexType(TBit)) : 0);

template <uint8 TMask, uint8... TBits>
static constexpr size_t TVertexSizeCalcExpander = (TVertexSizeCalcConditional<TMask, TBits> + ...);

} // namespace detail

template <uint8 TMask>
static constexpr size_t TVertexSize = detail::TVertexSizeCalcExpander<
    // TMask
    TMask,
    // TBits
    VT_Position,
    VT_Normal,
    VT_UV0,
    VT_UV1,
    VT_Skeletal>;

} // namespace VertexUtils

inline size_t VertexInputLayoutDesc::VertexSize() const
{
    size_t size = 0;

    FOR_EACH_BIT(mask, bit)
    {
        size += VertexUtils::PacketSize(VertexType(1 << bit));
    }

    return size;
}

} // namespace Hyperion
