/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MATERIAL_HPP
#define HYPERION_MATERIAL_HPP

#include <rendering/Shader.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/Mutex.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Color.hpp>

#include <Types.hpp>

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>

namespace hyperion {

class Texture;
class RenderMaterial;

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

HYP_CLASS()
class HYP_API Material final : public HypObject<Material>
{
    HYP_OBJECT_BODY(Material);

public:
    static constexpr uint32 max_parameters = 32u;
    static constexpr uint32 max_textures = 32u;

    friend class RenderMaterial;

    struct Parameter
    {
        union
        {
            float float_values[4];
            int int_values[4];
        } values;

        enum Type : uint32
        {
            MATERIAL_PARAMETER_TYPE_NONE,
            MATERIAL_PARAMETER_TYPE_FLOAT,
            MATERIAL_PARAMETER_TYPE_FLOAT2,
            MATERIAL_PARAMETER_TYPE_FLOAT3,
            MATERIAL_PARAMETER_TYPE_FLOAT4,
            MATERIAL_PARAMETER_TYPE_INT,
            MATERIAL_PARAMETER_TYPE_INT2,
            MATERIAL_PARAMETER_TYPE_INT3,
            MATERIAL_PARAMETER_TYPE_INT4
        } type;

        Parameter()
            : type(MATERIAL_PARAMETER_TYPE_NONE)
        {
            Memory::MemSet(&values, 0, sizeof(values));
        }

        template <SizeType Size>
        explicit Parameter(FixedArray<float, Size>&& v)
            : Parameter(v.Data(), Size)
        {
        }

        explicit Parameter(const float* v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_FLOAT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            Memory::MemCpy(values.float_values, v, count * sizeof(float));

            if (count < ArraySize(values.float_values))
            {
                Memory::MemSet(&values.float_values[count], 0, (ArraySize(values.float_values) - count) * sizeof(float));
            }
        }

        Parameter(float value)
            : Parameter(&value, 1)
        {
        }

        Parameter(const Vec2f& xy)
            : Parameter(xy.values, 2)
        {
        }

        Parameter(const Vec3f& xyz)
            : Parameter(xyz.values, 3)
        {
        }

        Parameter(const Vec4f& xyzw)
            : Parameter(xyzw.values, 4)
        {
        }

        Parameter(const Color& color)
            : Parameter(Vec4f(color.GetRed(), color.GetGreen(), color.GetBlue(), color.GetAlpha()))
        {
        }

        template <SizeType Size>
        explicit Parameter(FixedArray<int32, Size>&& v)
            : Parameter(v.Data(), Size)
        {
        }

        explicit Parameter(const int32* v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_INT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            Memory::MemCpy(values.int_values, v, count * sizeof(int32));

            if (count < ArraySize(values.int_values))
            {
                Memory::MemSet(&values.int_values[count], 0, (ArraySize(values.int_values) - count) * sizeof(int32));
            }
        }

        Parameter(int32 value)
            : Parameter(&value, 1)
        {
        }

        Parameter(const Vec2i& xy)
            : Parameter(xy.values, 2)
        {
        }

        Parameter(const Vec3i& xyz)
            : Parameter(xyz.values, 3)
        {
        }

        Parameter(const Vec4i& xyzw)
            : Parameter(xyzw.values, 4)
        {
        }

        Parameter(const Parameter& other)
            : type(other.type)
        {
            Memory::MemCpy(&values, &other.values, sizeof(values));
        }

        Parameter& operator=(const Parameter& other)
        {
            type = other.type;
            Memory::MemCpy(&values, &other.values, sizeof(values));

            return *this;
        }

        ~Parameter() = default;

        HYP_FORCE_INLINE bool IsIntType() const
        {
            return type >= MATERIAL_PARAMETER_TYPE_INT && type <= MATERIAL_PARAMETER_TYPE_INT4;
        }

        HYP_FORCE_INLINE bool IsFloatType() const
        {
            return type >= MATERIAL_PARAMETER_TYPE_FLOAT && type <= MATERIAL_PARAMETER_TYPE_FLOAT4;
        }

        HYP_FORCE_INLINE uint32 Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE)
            {
                return 0u;
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT)
            {
                return uint32(type - MATERIAL_PARAMETER_TYPE_INT) + 1;
            }

            return uint32(type);
        }

        HYP_FORCE_INLINE void Copy(uint8* dst) const
        {
            Memory::MemCpy(dst, &values, Size());
        }

        HYP_FORCE_INLINE bool operator==(const Parameter& other) const
        {
            return Memory::MemCmp(&values, &other.values, sizeof(values)) == 0;
        }

        HYP_FORCE_INLINE bool operator!=(const Parameter& other) const
        {
            return Memory::MemCmp(&values, &other.values, sizeof(values)) != 0;
        }

        HYP_FORCE_INLINE explicit operator int() const
        {
            return values.int_values[0];
        }

        HYP_FORCE_INLINE explicit operator Vec2i() const
        {
            return Vec2i { values.int_values[0], values.int_values[1] };
        }

        HYP_FORCE_INLINE explicit operator Vec3i() const
        {
            return Vec3i { values.int_values[0], values.int_values[1], values.int_values[2] };
        }

        HYP_FORCE_INLINE explicit operator Vec4i() const
        {
            return Vec4i { values.int_values[0], values.int_values[1], values.int_values[2], values.int_values[3] };
        }

        HYP_FORCE_INLINE explicit operator float() const
        {
            return values.float_values[0];
        }

        HYP_FORCE_INLINE explicit operator Vec2f() const
        {
            return Vec2f { values.float_values[0], values.float_values[1] };
        }

        HYP_FORCE_INLINE explicit operator Vec3f() const
        {
            return Vec3f { values.float_values[0], values.float_values[1], values.float_values[2] };
        }

        HYP_FORCE_INLINE explicit operator Vec4f() const
        {
            return Vec4f { values.float_values[0], values.float_values[1], values.float_values[2], values.float_values[3] };
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(int(type));

            if (IsIntType())
            {
                hc.Add(values.int_values[0]);
                hc.Add(values.int_values[1]);
                hc.Add(values.int_values[2]);
                hc.Add(values.int_values[3]);
            }
            else if (IsFloatType())
            {
                hc.Add(values.float_values[0]);
                hc.Add(values.float_values[1]);
                hc.Add(values.float_values[2]);
                hc.Add(values.float_values[3]);
            }

            return hc;
        }
    };

    enum State
    {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };

    enum MaterialKey : uint64
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

    using ParameterTable = EnumOptions<MaterialKey, Parameter, max_parameters>;

    class TextureSet : public EnumOptions<MaterialTextureKey, Handle<Texture>, max_textures>
    {
    public:
        TextureSet() = default;

        TextureSet(std::initializer_list<KeyValuePairType> initializer_list)
            : EnumOptions<MaterialTextureKey, Handle<Texture>, max_textures>(initializer_list)
        {
        }

        TextureSet(const HashMap<MaterialTextureKey, Handle<Texture>>& textures)
        {
            for (const auto& it : textures)
            {
                Set(it.first, it.second);
            }
        }

        TextureSet(HashMap<MaterialTextureKey, Handle<Texture>>&& textures)
        {
            for (auto& it : textures)
            {
                Set(it.first, std::move(it.second));
            }
        }

        TextureSet(const TextureSet& other)
            : EnumOptions<MaterialTextureKey, Handle<Texture>, max_textures>(other)
        {
        }

        TextureSet& operator=(const TextureSet& other)
        {
            if (this == &other)
            {
                return *this;
            }

            EnumOptions<MaterialTextureKey, Handle<Texture>, max_textures>::operator=(other);
            return *this;
        }

        TextureSet(TextureSet&& other) noexcept
            : EnumOptions<MaterialTextureKey, Handle<Texture>, max_textures>(std::move(other))
        {
        }

        TextureSet& operator=(TextureSet&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            EnumOptions<MaterialTextureKey, Handle<Texture>, max_textures>::operator=(std::move(other));
            return *this;
        }

        ~TextureSet() = default;

        HashMap<MaterialTextureKey, Handle<Texture>> ToHashMap() const
        {
            HashMap<MaterialTextureKey, Handle<Texture>> result;
            result.Reserve(Size());

            for (SizeType index = 0; index < Size(); ++index)
            {
                result.Insert(KeyValueAt(index));
            }

            return result;
        }

        Array<Handle<Texture>> ToArray() const
        {
            Array<Handle<Texture>> result;
            result.Resize(Size());

            for (SizeType index = 0; index < Size(); ++index)
            {
                result[index] = ValueAt(index);
            }

            return result;
        }
    };

    /*! \brief Default parameters for a Material. */
    static const ParameterTable& DefaultParameters();

    Material();
    Material(Name name, Bucket bucket = Bucket::BUCKET_OPAQUE);
    Material(Name name, const MaterialAttributes& attributes, const ParameterTable& parameters, const TextureSet& textures);
    Material(const Material& other) = delete;
    Material& operator=(const Material& other) = delete;
    Material(Material&& other) noexcept = delete;
    Material& operator=(Material&& other) noexcept = delete;
    ~Material() override;

    HYP_METHOD(Property = "Name", Serialize = true, Editor = true)
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_FORCE_INLINE RenderMaterial& GetRenderResource() const
    {
        return *m_render_resource;
    }

    /*! \brief Get the current mutation state of this Material.
        \return The current mutation state of this Material */
    HYP_FORCE_INLINE DataMutationState GetMutationState() const
    {
        return m_mutation_state;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    void SetShader(const ShaderRef& shader);

    HYP_FORCE_INLINE ParameterTable& GetParameters()
    {
        return m_parameters;
    }

    HYP_FORCE_INLINE const ParameterTable& GetParameters() const
    {
        return m_parameters;
    }

    HYP_FORCE_INLINE const Parameter& GetParameter(MaterialKey key) const
    {
        return m_parameters.Get(key);
    }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
    {
        return m_parameters.Get(key).values.float_values[0];
    }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, int>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
    {
        return m_parameters.Get(key).values.int_values[0];
    }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<decltype(T::values[0])>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
    {
        static_assert(sizeof(T::values) <= sizeof(Parameter::values), "T must have a size <= to the size of Parameter::values");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        T result;
        std::memcpy(&result.values[0], &m_parameters.Get(key).values.float_values[0], sizeof(float) * ArraySize(result.values));
        return result;
    }

    /*! \brief Set a parameter on this material with the given key and value
     *  \param key The key of the parameter to be set
     *  \param value The value of the parameter to be set */
    void SetParameter(MaterialKey key, const Parameter& value);

    /*! \brief Set all parameters on this Material to the given ParameterTable.
     *  \param parameters The ParameterTable to set. */
    void SetParameters(const ParameterTable& parameters);

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

    /*! \brief Sets all textures on this Material to the given TextureSet.
     *  If the Material has already been initialized, the Textures are initialized.
     *  Otherwise, they will be initialized when the Material is initialized.
     *  \param textures The TextureSet to set. */
    void SetTextures(const TextureSet& textures);

    HYP_FORCE_INLINE TextureSet& GetTextures()
    {
        return m_textures;
    }

    HYP_FORCE_INLINE const TextureSet& GetTextures() const
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
    HYP_FORCE_INLINE Bucket GetBucket() const
    {
        return m_render_attributes.bucket;
    }

    /*! \brief Set the bucket for this Material.
     *  \param bucket The bucket to set. */
    HYP_FORCE_INLINE void SetBucket(Bucket bucket)
    {
        m_render_attributes.bucket = bucket;
    }

    /*! \brief Get whether this Material is alpha blended.
     *  \return True if the Material is alpha blended, false otherwise. */
    HYP_FORCE_INLINE bool IsAlphaBlended() const
    {
        return m_render_attributes.blend_function != BlendFunction::None();
    }

    /*! \brief Set whether this Material is alpha blended.
     *  \param is_alpha_blended True if the Material is alpha blended, false otherwise.
     *  \param blend_function The blend function to use if the Material is alpha blended. By default, it is set to \ref{BlendFunction::AlphaBlending()}. */
    HYP_FORCE_INLINE void SetIsAlphaBlended(bool is_alpha_blended, BlendFunction blend_function = BlendFunction::AlphaBlending())
    {
        if (is_alpha_blended)
        {
            m_render_attributes.blend_function = blend_function;
        }
        else
        {
            m_render_attributes.blend_function = BlendFunction::None();
        }
    }

    /*! \brief Get the blend function for this Material.
     *  \return The blend function for this Material. */
    HYP_FORCE_INLINE BlendFunction GetBlendFunction() const
    {
        return m_render_attributes.blend_function;
    }

    /*! \brief Set the blend function for this Material.
     *  \param blend_function The blend function to set. */
    HYP_FORCE_INLINE void SetBlendMode(BlendFunction blend_function)
    {
        m_render_attributes.blend_function = blend_function;
    }

    /*! \brief Get whether depth writing is enabled for this Material.
     *  \return True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthWriteEnabled() const
    {
        return bool(m_render_attributes.flags & MaterialAttributeFlags::DEPTH_WRITE);
    }

    /*! \brief Set whether depth writing is enabled for this Material.
     *  \param is_depth_write_enabled True if depth writing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthWriteEnabled(bool is_depth_write_enabled)
    {
        if (is_depth_write_enabled)
        {
            m_render_attributes.flags |= MaterialAttributeFlags::DEPTH_WRITE;
        }
        else
        {
            m_render_attributes.flags &= ~MaterialAttributeFlags::DEPTH_WRITE;
        }
    }

    /*! \brief Get whether depth testing is enabled for this Material.
     *  \return True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE bool IsDepthTestEnabled() const
    {
        return bool(m_render_attributes.flags & MaterialAttributeFlags::DEPTH_TEST);
    }

    /*! \brief Set whether depth testing is enabled for this Material.
     *  \param is_depth_test_enabled True if depth testing is enabled, false otherwise. */
    HYP_FORCE_INLINE void SetIsDepthTestEnabled(bool is_depth_test_enabled)
    {
        if (is_depth_test_enabled)
        {
            m_render_attributes.flags |= MaterialAttributeFlags::DEPTH_TEST;
        }
        else
        {
            m_render_attributes.flags &= ~MaterialAttributeFlags::DEPTH_TEST;
        }
    }

    /*! \brief Get the face culling mode for this Material.
     *  \return The face culling mode for this Material. */
    HYP_FORCE_INLINE FaceCullMode GetFaceCullMode() const
    {
        return m_render_attributes.cull_faces;
    }

    /*! \brief Set the face culling mode for this Material.
     *  \param cull_mode The face culling mode to set. */
    HYP_FORCE_INLINE void SetFaceCullMode(FaceCullMode cull_mode)
    {
        m_render_attributes.cull_faces = cull_mode;
    }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE MaterialAttributes& GetRenderAttributes()
    {
        return m_render_attributes;
    }

    /*! \brief Get the render attributes of this Material.
     *  \return The render attributes of this Material. */
    HYP_FORCE_INLINE const MaterialAttributes& GetRenderAttributes() const
    {
        return m_render_attributes;
    }

    /*! \brief If a Material is static, it is expected to not change frequently and
     *  may be shared across many objects. Otherwise, it is considered dynamic and may
     *  be modified.
     *  \return True if the Material is static, false if it is dynamic. */
    HYP_METHOD()
    HYP_FORCE_INLINE bool IsStatic() const
    {
        return !m_is_dynamic;
    }

    /*! \brief If a Material is
     *  dynamic, it is expected to change frequently and may be modified. Otherwise,
     *  it is considered static and should not be modified as it may be shared across many
     *  objects.
     *  \return True if the Material is dynamic, false if it is static. */
    HYP_METHOD(Property = "IsDynamic", Serialize = true)
    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return m_is_dynamic;
    }

    HYP_METHOD(Property = "IsDynamic", Serialize = true)
    HYP_FORCE_INLINE void SetIsDynamic(bool is_dynamic)
    {
        m_is_dynamic = is_dynamic;
    }

    /*! \brief If the Material's mutation state is dirty, this will
     * create a task on the render thread to update the Material's
     * data on the GPU. */
    void EnqueueRenderUpdates();

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

private:
    void Init() override;

    Name m_name;

    ShaderRef m_shader;

    ParameterTable m_parameters;
    TextureSet m_textures;

    MaterialAttributes m_render_attributes;

    bool m_is_dynamic;

    mutable DataMutationState m_mutation_state;

    RenderMaterial* m_render_resource;
};

HYP_CLASS()
class MaterialGroup final : public HypObject<MaterialGroup>
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
        const Material::ParameterTable& parameters = Material::DefaultParameters(),
        const Material::TextureSet& textures = {});

    HYP_FORCE_INLINE Handle<Material> CreateMaterial(
        MaterialAttributes attributes = {},
        const Material::ParameterTable& parameters = Material::DefaultParameters(),
        const Material::TextureSet& textures = {})
    {
        return CreateMaterial(Name::Unique("Material"), attributes, parameters, textures);
    }

    Handle<Material> GetOrCreate(
        Name name,
        MaterialAttributes attributes = {},
        const Material::ParameterTable& parameters = Material::DefaultParameters(),
        const Material::TextureSet& textures = {});

    HYP_FORCE_INLINE Handle<Material> GetOrCreate(
        MaterialAttributes attributes = {},
        const Material::ParameterTable& parameters = Material::DefaultParameters(),
        const Material::TextureSet& textures = {})
    {
        return GetOrCreate(Name::Invalid(), attributes, parameters, textures);
    }

private:
    HashMap<HashCode, WeakHandle<Material>> m_map;
    Mutex m_mutex;
};

} // namespace hyperion

#endif
