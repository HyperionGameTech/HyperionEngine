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

#pragma pack(push, 1)

HYP_STRUCT()
class MaterialParameters
{
public:
    HYP_STRUCT_BODY(MaterialParameters);

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

    HYP_FIELD(Property = "UserParams", Serialize)
    Vec4f userParams;

    MaterialParameters()
        : albedo(1.0f),
          metalness(0.0f),
          roughness(1.0f),
          alphaThreshold(0.0f),
          parallaxHeightScale(0.02f),
          transmission(0.0f),
          ior(1.5f),
          emissiveColor(),
          emissiveIntensity(0.0f),
          userParams(0.0f)
    {
    }

    MaterialParameters(const MaterialParameters& other) = default;
    MaterialParameters& operator=(const MaterialParameters& other) = default;

    MaterialParameters(MaterialParameters&& other) noexcept = default;
    MaterialParameters& operator=(MaterialParameters&& other) noexcept = default;

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
};

#pragma pack(pop)

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
    HYP_FIELD(Serialize)
    FixedArray<Handle<Texture>, MaxTextures> m_values;
};

} // namespace Hyperion
