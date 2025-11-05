/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>

#include <core/reflection/HypObjectFwd.hpp>

#include <core/math/Color.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <asset/AssetObject.hpp>

#include <util/EnumOptions.hpp>

namespace hyperion {

class Texture;
class RenderProxyMaterial;

HYP_ENUM()
enum class MaterialTextureKey : uint64
{
    NONE = 0,

    ALBEDO_MAP = 1 << 0,
    NORMAL_MAP = 1 << 1,
    AO_MAP = 1 << 2,
    PARALLAX_MAP = 1 << 3,
    METALNESS_MAP = 1 << 4,
    ROUGHNESS_MAP = 1 << 5,
    RADIANCE_MAP = 1 << 6,
    IRRADIANCE_MAP = 1 << 7,
    RESERVED0 = 1 << 8,
    RESERVED1 = 1 << 9,
    RESERVED2 = 1 << 10,
    RESERVED3 = 1 << 11,
    RESERVED4 = 1 << 12,
    RESERVED5 = 1 << 13,

    // terrain

    SPLAT_MAP = 1 << 14,

    BASE_TERRAIN_COLOR_MAP = 1 << 15,
    BASE_TERRAIN_NORMAL_MAP = 1 << 16,
    BASE_TERRAIN_AO_MAP = 1 << 17,
    BASE_TERRAIN_PARALLAX_MAP = 1 << 18,

    TERRAIN_LEVEL1_COLOR_MAP = 1 << 19,
    TERRAIN_LEVEL1_NORMAL_MAP = 1 << 20,
    TERRAIN_LEVEL1_AO_MAP = 1 << 21,
    TERRAIN_LEVEL1_PARALLAX_MAP = 1 << 22,

    TERRAIN_LEVEL2_COLOR_MAP = 1 << 23,
    TERRAIN_LEVEL2_NORMAL_MAP = 1 << 24,
    TERRAIN_LEVEL2_AO_MAP = 1 << 25,
    TERRAIN_LEVEL2_PARALLAX_MAP = 1 << 26
};

HYP_ENUM()
enum MaterialParameterType : uint32
{
    MPT_NONE = 0,
    MPT_FLOAT = 1,
    MPT_FLOAT2 = 2,
    MPT_FLOAT3 = 3,
    MPT_FLOAT4 = 4,
    MPT_INT = 5,
    MPT_INT2 = 6,
    MPT_INT3 = 7,
    MPT_INT4 = 8
};

HYP_STRUCT(Serialize = "bitwise", Size = 16)
struct MaterialParameterValue
{
    HYP_STRUCT_BODY(MaterialParameterValue);

    union
    {
        float floatValues[4];
        int32 intValues[4];
    };
};

HYP_STRUCT()
struct MaterialParameter
{
    HYP_STRUCT_BODY(MaterialParameter);

    using Type = MaterialParameterType;

    HYP_FIELD(Serialize)
    MaterialParameterValue value;

    HYP_FIELD(Serialize)
    MaterialParameterType type;

    MaterialParameter()
        : type(MPT_NONE)
    {
        Memory::MemSet(&value, 0, sizeof(value));
    }

    template <SizeType Size>
    explicit MaterialParameter(FixedArray<float, Size>&& v)
        : MaterialParameter(v.Data(), Size)
    {
    }

    explicit MaterialParameter(const float* v, SizeType count)
        : type(Type(MPT_FLOAT + (count - 1)))
    {
        Assert(count >= 1 && count <= 4);

        Memory::MemCpy(value.floatValues, v, count * sizeof(float));

        if (count < ArraySize(value.floatValues))
        {
            Memory::MemSet(&value.floatValues[count], 0, (ArraySize(value.floatValues) - count) * sizeof(float));
        }
    }

    MaterialParameter(float value)
        : MaterialParameter(&value, 1)
    {
    }

    MaterialParameter(const Vec2f& xy)
        : MaterialParameter(xy.values, 2)
    {
    }

    MaterialParameter(const Vec3f& xyz)
        : MaterialParameter(xyz.values, 3)
    {
    }

    MaterialParameter(const Vec4f& xyzw)
        : MaterialParameter(xyzw.values, 4)
    {
    }

    MaterialParameter(const Color& color)
        : MaterialParameter(Vec4f(color.GetRed(), color.GetGreen(), color.GetBlue(), color.GetAlpha()))
    {
    }

    template <SizeType Size>
    explicit MaterialParameter(FixedArray<int32, Size>&& v)
        : MaterialParameter(v.Data(), Size)
    {
    }

    explicit MaterialParameter(const int32* v, SizeType count)
        : type(Type(MPT_INT + (count - 1)))
    {
        Assert(count >= 1 && count <= 4);

        Memory::MemCpy(value.intValues, v, count * sizeof(int32));

        if (count < ArraySize(value.intValues))
        {
            Memory::MemSet(&value.intValues[count], 0, (ArraySize(value.intValues) - count) * sizeof(int32));
        }
    }

    MaterialParameter(int32 value)
        : MaterialParameter(&value, 1)
    {
    }

    MaterialParameter(const Vec2i& xy)
        : MaterialParameter(xy.values, 2)
    {
    }

    MaterialParameter(const Vec3i& xyz)
        : MaterialParameter(xyz.values, 3)
    {
    }

    MaterialParameter(const Vec4i& xyzw)
        : MaterialParameter(xyzw.values, 4)
    {
    }

    MaterialParameter(const MaterialParameter& other)
        : type(other.type)
    {
        Memory::MemCpy(&value, &other.value, sizeof(value));
    }

    MaterialParameter& operator=(const MaterialParameter& other)
    {
        type = other.type;
        Memory::MemCpy(&value, &other.value, sizeof(value));

        return *this;
    }

    ~MaterialParameter() = default;

    HYP_FORCE_INLINE bool IsIntType() const
    {
        return type >= MPT_INT && type <= MPT_INT4;
    }

    HYP_FORCE_INLINE bool IsFloatType() const
    {
        return type >= MPT_FLOAT && type <= MPT_FLOAT4;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        if (type == MPT_NONE)
        {
            return 0u;
        }

        if (type >= MPT_INT)
        {
            return uint32(type - MPT_INT) + 1;
        }

        return uint32(type);
    }

    HYP_FORCE_INLINE void Copy(uint8* dst) const
    {
        Memory::MemCpy(dst, &value, Size());
    }

    HYP_FORCE_INLINE bool operator==(const MaterialParameter& other) const
    {
        return Memory::MemCmp(&value, &other.value, sizeof(value)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialParameter& other) const
    {
        return Memory::MemCmp(&value, &other.value, sizeof(value)) != 0;
    }

    HYP_FORCE_INLINE explicit operator int() const
    {
        return value.intValues[0];
    }

    HYP_FORCE_INLINE explicit operator Vec2i() const
    {
        return Vec2i { value.intValues[0], value.intValues[1] };
    }

    HYP_FORCE_INLINE explicit operator Vec3i() const
    {
        return Vec3i { value.intValues[0], value.intValues[1], value.intValues[2] };
    }

    HYP_FORCE_INLINE explicit operator Vec4i() const
    {
        return Vec4i { value.intValues[0], value.intValues[1], value.intValues[2], value.intValues[3] };
    }

    HYP_FORCE_INLINE explicit operator float() const
    {
        return value.floatValues[0];
    }

    HYP_FORCE_INLINE explicit operator Vec2f() const
    {
        return Vec2f { value.floatValues[0], value.floatValues[1] };
    }

    HYP_FORCE_INLINE explicit operator Vec3f() const
    {
        return Vec3f { value.floatValues[0], value.floatValues[1], value.floatValues[2] };
    }

    HYP_FORCE_INLINE explicit operator Vec4f() const
    {
        return Vec4f { value.floatValues[0], value.floatValues[1], value.floatValues[2], value.floatValues[3] };
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(int(type));
        hc.Add(HashCode::GetHashCode(reinterpret_cast<const ubyte*>(&value), reinterpret_cast<const ubyte*>(&value) + sizeof(value)));

        return hc;
    }
};

HYP_ENUM()
enum MaterialParameterKey : uint64
{
    MATERIAL_KEY_NONE = 0,

    // basic
    MATERIAL_KEY_ALBEDO = 1 << 0,
    MATERIAL_KEY_METALNESS = 1 << 1,
    MATERIAL_KEY_ROUGHNESS = 1 << 2,
    MATERIAL_KEY_TRANSMISSION = 1 << 3,
    MATERIAL_KEY_EMISSIVE = 1 << 4,         // UNUSED
    MATERIAL_KEY_SPECULAR = 1 << 5,         // UNUSED
    MATERIAL_KEY_SPECULAR_TINT = 1 << 6,    // UNUSED
    MATERIAL_KEY_ANISOTROPIC = 1 << 7,      // UNUSED
    MATERIAL_KEY_SHEEN = 1 << 8,            // UNUSED
    MATERIAL_KEY_SHEEN_TINT = 1 << 9,       // UNUSED
    MATERIAL_KEY_CLEARCOAT = 1 << 10,       // UNUSED
    MATERIAL_KEY_CLEARCOAT_GLOSS = 1 << 11, // UNUSED
    MATERIAL_KEY_SUBSURFACE = 1 << 12,      // UNUSED
    MATERIAL_KEY_NORMAL_MAP_INTENSITY = 1 << 13,
    MATERIAL_KEY_UV_SCALE = 1 << 14,
    MATERIAL_KEY_PARALLAX_HEIGHT = 1 << 15,
    MATERIAL_KEY_ALPHA_THRESHOLD = 1 << 16,
    MATERIAL_KEY_RESERVED2 = 1 << 17,

    // terrain
    MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT = 1 << 18,
    MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT = 1 << 19,
    MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT = 1 << 20,
    MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT = 1 << 21
};

HYP_STRUCT()
class MaterialParameters
{
public:
    HYP_STRUCT_BODY(MaterialParameters);

    static constexpr uint32 MaxParameters = 32u;

    using Iterator = FixedArray<MaterialParameter, MaxParameters>::Iterator;
    using ConstIterator = FixedArray<MaterialParameter, MaxParameters>::ConstIterator;

    MaterialParameters() = default;

    MaterialParameters(std::initializer_list<Pair<MaterialParameterKey, MaterialParameter>> initializerList)
    {
        for (const auto& it : initializerList)
        {
            m_values[EnumOptions<MaterialParameterKey, MaterialParameter, MaxParameters>::EnumToOrdinal(it.first)] = it.second;
        }
    }

    MaterialParameters(const MaterialParameters& other) = default;
    MaterialParameters& operator=(const MaterialParameters& other) = default;

    MaterialParameters(MaterialParameters&& other) noexcept = default;
    MaterialParameters& operator=(MaterialParameters&& other) noexcept = default;

    ~MaterialParameters() = default;

    HYP_FORCE_INLINE bool operator==(const MaterialParameters& other) const
    {
        return m_values == other.m_values;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialParameters& other) const
    {
        return m_values != other.m_values;
    }

    HYP_FORCE_INLINE MaterialParameter& operator[](MaterialParameterKey key)
    {
        return m_values[EnumOptions<MaterialParameterKey, MaterialParameter, MaxParameters>::EnumToOrdinal(key)];
    }

    HYP_FORCE_INLINE const MaterialParameter& operator[](MaterialParameterKey key) const
    {
        return m_values[EnumOptions<MaterialParameterKey, MaterialParameter, MaxParameters>::EnumToOrdinal(key)];
    }

    HYP_FORCE_INLINE MaterialParameter& AtIndex(SizeType index)
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE const MaterialParameter& AtIndex(SizeType index) const
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE Pair<MaterialParameterKey, MaterialParameter&> KeyValueAt(SizeType index)
    {
        return Pair<MaterialParameterKey, MaterialParameter&>(
            EnumOptions<MaterialParameterKey, MaterialParameter, MaxParameters>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE Pair<MaterialParameterKey, const MaterialParameter&> KeyValueAt(SizeType index) const
    {
        return Pair<MaterialParameterKey, const MaterialParameter&>(
            EnumOptions<MaterialParameterKey, MaterialParameter, MaxParameters>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE SizeType Size() const
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
    FixedArray<MaterialParameter, MaxParameters> m_values;
};

HYP_STRUCT()
class MaterialTextures
{
public:
    HYP_STRUCT_BODY(MaterialTextures);

    static constexpr uint32 MaxTextures = 32u;

    using Iterator = FixedArray<Handle<Texture>, MaxTextures>::Iterator;
    using ConstIterator = FixedArray<Handle<Texture>, MaxTextures>::ConstIterator;

    MaterialTextures() = default;

    MaterialTextures(std::initializer_list<Pair<MaterialTextureKey, Handle<Texture>>> initializerList)
    {
        for (const auto& it : initializerList)
        {
            m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(it.first)] = it.second;
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

    HYP_FORCE_INLINE Handle<Texture>& operator[](MaterialTextureKey key)
    {
        return m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key)];
    }

    HYP_FORCE_INLINE const Handle<Texture>& operator[](MaterialTextureKey key) const
    {
        return m_values[EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::EnumToOrdinal(key)];
    }

    HYP_FORCE_INLINE Handle<Texture>& AtIndex(SizeType index)
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE const Handle<Texture>& AtIndex(SizeType index) const
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE Pair<MaterialTextureKey, Handle<Texture>&> KeyValueAt(SizeType index)
    {
        return Pair<MaterialTextureKey, Handle<Texture>&>(
            EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE Pair<MaterialTextureKey, const Handle<Texture>&> KeyValueAt(SizeType index) const
    {
        return Pair<MaterialTextureKey, const Handle<Texture>&>(
            EnumOptions<MaterialTextureKey, Handle<Texture>, MaxTextures>::OrdinalToEnum(index),
            m_values[index]);
    }

    HYP_FORCE_INLINE SizeType Size() const
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

HYP_CLASS()
class HYP_API Material final : public AssetObject
{
    HYP_OBJECT_BODY(Material);

public:
    enum State
    {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };

    /*! \brief Default parameters for a Material. */
    static const MaterialParameters& DefaultParameters();

    Material();

    Material(
        Name name,
        RenderBucket rb = RB_OPAQUE);

    Material(
        Name name,
        const MaterialAttributes& attributes);

    Material(
        Name name,
        const MaterialAttributes& attributes,
        const MaterialParameters& parameters,
        const MaterialTextures& textures);

    Material(const Material& other) = delete;
    Material& operator=(const Material& other) = delete;

    Material(Material&& other) noexcept = delete;
    Material& operator=(Material&& other) noexcept = delete;

    ~Material() override;

    /*! \brief Get the current mutation state of this Material.
        \return The current mutation state of this Material */
    HYP_FORCE_INLINE DataMutationState GetMutationState() const
    {
        return m_mutationState;
    }

    HYP_FORCE_INLINE MaterialParameters& GetParameters()
    {
        return m_parameters;
    }

    HYP_FORCE_INLINE const MaterialParameters& GetParameters() const
    {
        return m_parameters;
    }

    HYP_FORCE_INLINE const MaterialParameter& GetParameter(MaterialParameterKey key) const
    {
        return m_parameters[key];
    }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, float>, std::decay_t<T>>
    GetParameter(MaterialParameterKey key) const
    {
        return m_parameters[key].value.floatValues[0];
    }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, int>, std::decay_t<T>>
    GetParameter(MaterialParameterKey key) const
    {
        return m_parameters[key].value.intValues[0];
    }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<decltype(T::values[0])>, float>, std::decay_t<T>>
    GetParameter(MaterialParameterKey key) const
    {
        static_assert(sizeof(T::values) <= sizeof(MaterialParameter::value), "T must have a size <= to the size of Parameter::values");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        T result;
        std::memcpy(&result.values[0], &m_parameters[key].value.floatValues[0], sizeof(float) * ArraySize(result.values));
        return result;
    }

    /*! \brief Set a parameter on this material with the given key and value
     *  \param key The key of the parameter to be set
     *  \param value The value of the parameter to be set */
    void SetParameter(MaterialParameterKey key, const MaterialParameter& value);

    /*! \brief Set all parameters on this Material to the given MaterialParameters.
     *  \param parameters The material parameter table to set. */
    void SetParameters(const MaterialParameters& parameters);

    /*! \brief Set all parameters back to their default values. */
    void ResetParameters();

    /*! \brief Sets the texture with the given key on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param key The texture slot to set the texture on
     *  \param texture The Texture handle to set. */
    void SetTexture(MaterialTextureKey key, const Handle<Texture>& texture);

    /*! \brief Sets the texture at the given index on this Material.
     *  If the Material has already been initialized, the Texture is initialized.
     *  Otherwise, it will be initialized when the Material is initialized.
     *  \param index The index to set the texture in
     *  \param texture The Texture handle to set. */
    void SetTextureAtIndex(uint32 index, const Handle<Texture>& texture);

    /*! \brief Sets all textures on this Material to the given MaterialTextures.
     *  If the Material has already been initialized, the Textures are initialized.
     *  Otherwise, they will be initialized when the Material is initialized.
     *  \param textures The textures to set on this Material. */
    void SetTextures(const MaterialTextures& textures);

    HYP_FORCE_INLINE MaterialTextures& GetTextures()
    {
        return m_textures;
    }

    HYP_FORCE_INLINE const MaterialTextures& GetTextures() const
    {
        return m_textures;
    }

    /*! \brief Return a pointer to a Texture set on this Material by the given
     *  texture key. If no Texture was set, nullptr is returned.
     *  \param key The key of the texture to find
     *  \return Handle for the found Texture, or an empty Handle if not found. */
    const Handle<Texture>& GetTexture(MaterialTextureKey key) const;

    /*! \brief Return a pointer to a Texture set on this Material by the given
     *  index. If no Texture was set, nullptr is returned.
     *  \param index The index of the texture to find
     *  \return Handle for the found Texture, or an empty Handle if not found. */
    const Handle<Texture>& GetTextureAtIndex(uint32 index) const;

    /*! \brief Get the bucket for this Material.
     *  \return The bucket for this Material. */
    HYP_FORCE_INLINE RenderBucket GetBucket() const
    {
        return m_attributes.bucket;
    }

    /*! \brief Set the bucket for this Material.
     *  \param rb The bucket to set. */
    HYP_FORCE_INLINE void SetBucket(RenderBucket rb)
    {
        m_attributes.bucket = rb;
    }

    /*! \brief Get whether this Material is alpha blended.
     *  \return True if the Material is alpha blended, false otherwise. */
    HYP_FORCE_INLINE bool IsAlphaBlended() const
    {
        return m_attributes.blendFunction != BlendFunction::None();
    }

    /*! \brief Set whether this Material is alpha blended.
     *  \param isAlphaBlended True if the Material is alpha blended, false otherwise.
     *  \param blendFunction The blend function to use if the Material is alpha blended. By default, it is set to \ref{BlendFunction::AlphaBlending()}. */
    HYP_FORCE_INLINE void SetIsAlphaBlended(bool isAlphaBlended, BlendFunction blendFunction = BlendFunction::AlphaBlending())
    {
        if (isAlphaBlended)
        {
            m_attributes.blendFunction = blendFunction;
        }
        else
        {
            m_attributes.blendFunction = BlendFunction::None();
        }
    }

    /*! \brief Get the blend function for this Material.
     *  \return The blend function for this Material. */
    HYP_FORCE_INLINE BlendFunction GetBlendFunction() const
    {
        return m_attributes.blendFunction;
    }

    /*! \brief Set the blend function for this Material.
     *  \param blendFunction The blend function to set. */
    HYP_FORCE_INLINE void SetBlendMode(BlendFunction blendFunction)
    {
        m_attributes.blendFunction = blendFunction;
    }

    /*! \brief Get whether depth writing is enabled for this Material.
     *  \return True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthWriteEnabled() const
    {
        return bool(m_attributes.flags & MAF_DEPTH_WRITE);
    }

    /*! \brief Set whether depth writing is enabled for this Material.
     *  \param isDepthWriteEnabled True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthWriteEnabled(bool isDepthWriteEnabled)
    {
        if (isDepthWriteEnabled)
        {
            m_attributes.flags |= MAF_DEPTH_WRITE;
        }
        else
        {
            m_attributes.flags &= ~MAF_DEPTH_WRITE;
        }
    }

    /*! \brief Get whether depth testing is enabled for this Material.
     *  \return True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthTestEnabled() const
    {
        return bool(m_attributes.flags & MAF_DEPTH_TEST);
    }

    /*! \brief Set whether depth testing is enabled for this Material.
     *  \param isDepthTestEnabled True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthTestEnabled(bool isDepthTestEnabled)
    {
        if (isDepthTestEnabled)
        {
            m_attributes.flags |= MAF_DEPTH_TEST;
        }
        else
        {
            m_attributes.flags &= ~MAF_DEPTH_TEST;
        }
    }

    /*! \brief Get the face culling mode for this Material.
     *  \return The face culling mode for this Material. */
    HYP_FORCE_INLINE FaceCullMode GetFaceCullMode() const
    {
        return m_attributes.cullFaces;
    }

    /*! \brief Set the face culling mode for this Material.
     *  \param cullMode The face culling mode to set. */
    HYP_FORCE_INLINE void SetFaceCullMode(FaceCullMode cullMode)
    {
        m_attributes.cullFaces = cullMode;
    }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE MaterialAttributes& GetRenderAttributes()
    {
        return m_attributes;
    }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE const MaterialAttributes& GetRenderAttributes() const
    {
        return m_attributes;
    }

    /*! \brief If a Material is static, it is expected to not change frequently and
     *  may be shared across many objects. Otherwise, it is considered dynamic and may
     *  be modified.
     *  \return True if the Material is static, false if it is dynamic. */
    HYP_METHOD()
    HYP_FORCE_INLINE bool IsStatic() const
    {
        return !m_isDynamic;
    }

    /*! \brief If a Material is
     *  dynamic, it is expected to change frequently and may be modified. Otherwise,
     *  it is considered static and should not be modified as it may be shared across many
     *  objects.
     *  \return True if the Material is dynamic, false if it is static. */
    HYP_METHOD()
    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return m_isDynamic;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetIsDynamic(bool isDynamic)
    {
        m_isDynamic = isDynamic;
    }

    /*! \brief If the Material's mutation state is dirty, this will
     * create a task on the render thread to update the Material's
     * data on the GPU. */
    void EnqueueRenderUpdates();

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

    void UpdateRenderProxy(RenderProxyMaterial* proxy);

    /*! \brief Clone this Material. The cloned Material will have the same
     *  shader, parameters, textures, and render attributes as the original.
     *  \details Using this method is a good way to get around the fact that
     *  static Materials are shared across many objects. If you need to modify
     *  a static Material, clone it first. The cloned Material will be dynamic
     *  by default, and can be modified without affecting the original Material.
     *  \note The cloned Material will not be initialized.
     *  \return A new Material that is a clone of this Material. */
    HYP_METHOD()
    Handle<Material> Clone() const;

    HashCode GetHashCode() const;

    bool m_debugIsDestroyed = false;

private:
    void Init() override;

    HYP_FIELD()
    MaterialParameters m_parameters;

    HYP_FIELD()
    MaterialTextures m_textures;

    HYP_FIELD()
    MaterialAttributes m_attributes;

    HYP_FIELD()
    bool m_isDynamic;

    mutable DataMutationState m_mutationState;

    int m_renderProxyVersion;
};

HYP_CLASS()
class MaterialGroup final : public HypObjectBase
{
    HYP_OBJECT_BODY(MaterialGroup);

public:
    MaterialGroup();
    MaterialGroup(const MaterialGroup& other) = delete;
    MaterialGroup& operator=(const MaterialGroup& other) = delete;
    ~MaterialGroup() override;

    void Add(const String& name, Handle<Material>&& material);
    bool Remove(const String& name);

    Handle<Material>& Get(const String& name)
    {
        return m_materials[name];
    }

    const Handle<Material>& Get(const String& name) const
    {
        return m_materials.At(name);
    }

    bool Has(const String& name) const
    {
        return m_materials.Contains(name);
    }

private:
    void Init() override;

    HashMap<String, Handle<Material>> m_materials;
};

class HYP_API MaterialCache
{
public:
    static MaterialCache* GetInstance();

    void Add(const Handle<Material>& material);

    Handle<Material> CreateMaterial(
        Name name,
        MaterialAttributes attributes = {},
        const MaterialParameters& parameters = Material::DefaultParameters(),
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<Material> CreateMaterial(
        MaterialAttributes attributes = {},
        const MaterialParameters& parameters = Material::DefaultParameters(),
        const MaterialTextures& textures = {})
    {
        return CreateMaterial(Name::Unique("Material"), attributes, parameters, textures);
    }

    Handle<Material> GetOrCreate(
        Name name,
        MaterialAttributes attributes = {},
        const MaterialParameters& parameters = Material::DefaultParameters(),
        const MaterialTextures& textures = {});

    HYP_FORCE_INLINE Handle<Material> GetOrCreate(
        MaterialAttributes attributes = {},
        const MaterialParameters& parameters = Material::DefaultParameters(),
        const MaterialTextures& textures = {})
    {
        return GetOrCreate(Name::Invalid(), attributes, parameters, textures);
    }

private:
    HashMap<HashCode, WeakHandle<Material>> m_map;
    Mutex m_mutex;
};

} // namespace hyperion
