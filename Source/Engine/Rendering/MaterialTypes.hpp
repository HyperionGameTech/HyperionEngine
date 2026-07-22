/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderableAttributes.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Math/Color.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Util/EnumOptions.hpp>

#include <initializer_list>

namespace Hyperion {

class Texture;

HYP_ENUM()
enum class MaterialTextureKey : uint64
{
    NONE = 0,

    Diffuse = 0x1,
    Normals = 0x2,
    Parallax = 0x4,
    Metalness = 0x8,
    Roughness = 0x10,
    AmbientOcclusion = 0x20
};

HYP_ENUM()
enum class MaterialTextureChannel : uint8
{
    R = 0,
    G = 1,
    B = 2,
    A = 3
};

HYP_STRUCT()
class MaterialParameters
{
public:
    HYP_STRUCT_BODY(MaterialParameters);

    static constexpr uint8 FlagBit_NormalMapFlipY = 0x1u;
    static constexpr uint8 FlagShift_RoughnessChannel = 1;
    static constexpr uint8 FlagShift_MetalnessChannel = 3;
    static constexpr uint8 FlagShift_AmbientOcclusionChannel = 5;
    static constexpr uint8 FlagMask_Channel = 0x3u;

    HYP_FIELD(Property = "Albedo", Editor, Serialize)
    Vec4f albedo;

    HYP_FIELD(Property = "Metalness", Editor, Serialize)
    float metalness;

    HYP_FIELD(Property = "Roughness", Editor, Serialize)
    float roughness;

    HYP_FIELD(Property = "AlphaThreshold", Editor, Serialize)
    float alphaThreshold;

    HYP_FIELD(Property = "ParallaxHeightScale", Editor, Serialize)
    float parallaxHeightScale;

    HYP_FIELD(Property = "Transmission", Editor, Serialize)
    float transmission;

    HYP_FIELD(Property = "IOR", Editor, Serialize)
    float ior;

    HYP_FIELD(Property = "EmissiveColor", Editor, Serialize)
    Color emissiveColor;

    HYP_FIELD(Property = "EmissiveIntensity", Editor, Serialize)
    float emissiveIntensity;

    HYP_FIELD(Property = "UserParams", Editor, Serialize)
    Vec4f userParams;

    HYP_FIELD(Property = "UVScale", Editor, Serialize)
    Vec2f uvScale;

    HYP_FIELD(Property = "Unlit", Editor, Serialize)
    bool unlit;

    HYP_FIELD(Property = "Flags", Serialize, Editor = false)
    uint8 flags;

    enum NoInitTag { NoInit };

    MaterialParameters()
    {
        static const MaterialParameters s_defaults = Defaults();
        memcpy(this, &s_defaults, sizeof(MaterialParameters));
    }

    explicit MaterialParameters(NoInitTag)
    {
    }

    MaterialParameters(const MaterialParameters& other) = default;
    MaterialParameters& operator=(const MaterialParameters& other) = default;

    HYP_FORCE_INLINE bool operator==(const MaterialParameters& other) const
    {
        return memcmp(this, &other, sizeof(MaterialParameters)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialParameters& other) const
    {
        return memcmp(this, &other, sizeof(MaterialParameters)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode((const ubyte*)this, (const ubyte*)this + sizeof(MaterialParameters));
    }

    HYP_METHOD(Property = "NormalMapFlipY", Editor = false)
    HYP_FORCE_INLINE bool IsNormalMapFlipY() const
    {
        return (flags & FlagBit_NormalMapFlipY) != 0;
    }

    HYP_METHOD(Property = "NormalMapFlipY", Editor = false)
    HYP_FORCE_INLINE void SetNormalMapFlipY(bool value)
    {
        flags = value
            ? static_cast<uint8>(flags | FlagBit_NormalMapFlipY)
            : static_cast<uint8>(flags & ~FlagBit_NormalMapFlipY);
    }

    HYP_METHOD(Property = "RoughnessChannel", Editor = false)
    HYP_FORCE_INLINE MaterialTextureChannel GetRoughnessChannel() const
    {
        return MaterialTextureChannel((flags >> FlagShift_RoughnessChannel) & FlagMask_Channel);
    }

    HYP_METHOD(Property = "RoughnessChannel", Editor = false)
    HYP_FORCE_INLINE void SetRoughnessChannel(MaterialTextureChannel channel)
    {
        flags = static_cast<uint8>((flags & ~(FlagMask_Channel << FlagShift_RoughnessChannel))
            | ((uint8(channel) & FlagMask_Channel) << FlagShift_RoughnessChannel));
    }

    HYP_METHOD(Property = "MetalnessChannel", Editor = false)
    HYP_FORCE_INLINE MaterialTextureChannel GetMetalnessChannel() const
    {
        return MaterialTextureChannel((flags >> FlagShift_MetalnessChannel) & FlagMask_Channel);
    }

    HYP_METHOD(Property = "MetalnessChannel", Editor = false)
    HYP_FORCE_INLINE void SetMetalnessChannel(MaterialTextureChannel channel)
    {
        flags = static_cast<uint8>((flags & ~(FlagMask_Channel << FlagShift_MetalnessChannel))
            | ((uint8(channel) & FlagMask_Channel) << FlagShift_MetalnessChannel));
    }

    HYP_METHOD(Property = "AmbientOcclusionChannel", Editor = false)
    HYP_FORCE_INLINE MaterialTextureChannel GetAmbientOcclusionChannel() const
    {
        return MaterialTextureChannel((flags >> FlagShift_AmbientOcclusionChannel) & FlagMask_Channel);
    }

    HYP_METHOD(Property = "AmbientOcclusionChannel", Editor = false)
    HYP_FORCE_INLINE void SetAmbientOcclusionChannel(MaterialTextureChannel channel)
    {
        flags = static_cast<uint8>((flags & ~(FlagMask_Channel << FlagShift_AmbientOcclusionChannel))
            | ((uint8(channel) & FlagMask_Channel) << FlagShift_AmbientOcclusionChannel));
    }

    static MaterialParameters Defaults()
    {
        MaterialParameters defaults(NoInit);
        memset(&defaults, 0, sizeof(MaterialParameters));

        defaults.albedo = Vec4f::One();
        defaults.roughness = 1.0f;
        defaults.parallaxHeightScale = 0.02f;
        defaults.ior = 1.5f;
        defaults.uvScale = Vec2f::One();

        return defaults;
    }
};

static_assert(std::is_trivially_destructible_v<MaterialParameters> && std::is_trivially_copyable_v<MaterialParameters>);


HYP_STRUCT()
class MaterialTextures
{
public:
    HYP_STRUCT_BODY(MaterialTextures);

    static constexpr uint32 MaxTextures = 8;

    using Iterator = FixedArray<Handle<Texture>, MaxTextures>::Iterator;
    using ConstIterator = FixedArray<Handle<Texture>, MaxTextures>::ConstIterator;

    MaterialTextures() = default;

    MaterialTextures(std::initializer_list<Pair<MaterialTextureKey, Handle<Texture>>> initializerList)
    {
        for (const auto& it : initializerList)
        {
            const size_t ord = EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(it.first);

            m_values[ord] = it.second;
        }
    }

    MaterialTextures(const MaterialTextures& other) = default;
    MaterialTextures& operator=(const MaterialTextures& other) = default;

    MaterialTextures(MaterialTextures&& other) noexcept = default;
    MaterialTextures& operator=(MaterialTextures&& other) noexcept = default;

    ~MaterialTextures() = default;

    HYP_FORCE_INLINE bool operator==(const MaterialTextures& other) const
    {
        return m_values == other.m_values;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialTextures& other) const
    {
        return m_values != other.m_values;
    }

    HYP_FORCE_INLINE Iterator Find(MaterialTextureKey key)
    {
        const size_t ord = EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key);

        if (ord >= m_values.Size())
        {
            return End();
        }

        return m_values.Begin() + ord;
    }

    HYP_FORCE_INLINE ConstIterator Find(MaterialTextureKey key) const
    {
        return const_cast<MaterialTextures*>(this)->Find(key);
    }

    HYP_FORCE_INLINE Handle<Texture>& operator[](MaterialTextureKey key)
    {
        const size_t ord = EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key);
        return m_values[ord];
    }

    HYP_FORCE_INLINE const Handle<Texture>& operator[](MaterialTextureKey key) const
    {
        return (*const_cast<MaterialTextures*>(this))[key];
    }

    HYP_FORCE_INLINE bool Has(MaterialTextureKey key) const
    {
        const size_t ord = EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key);

        if (ord >= m_values.Size())
        {
            return false;
        }

        return bool(m_values[ord]);
    }

    HYP_FORCE_INLINE Handle<Texture>& AtIndex(size_t index)
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE const Handle<Texture>& AtIndex(size_t index) const
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE Pair<MaterialTextureKey, Handle<Texture>&> KeyValueAt(size_t index)
    {
        return Pair<MaterialTextureKey, Handle<Texture>&>(
            EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE Pair<MaterialTextureKey, const Handle<Texture>&> KeyValueAt(size_t index) const
    {
        return Pair<MaterialTextureKey, const Handle<Texture>&>(
            EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_values.Size();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return m_values.GetHashCode();
    }

    HYP_DEF_STL_BEGIN_END(m_values.Begin(), m_values.End());

private:
    HYP_FIELD(Property = "Values", Serialize, Editor)
    FixedArray<Handle<Texture>, MaxTextures> m_values;
};

} // namespace Hyperion
