#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include <rendering/Texture.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/ShaderDataState.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <core/lib/FixedArray.hpp>
#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <Types.hpp>

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>

#include <array>
#include <mutex>

namespace hyperion {
namespace renderer {

class DescriptorSet;

} // namespace renderer
} // namespace hyperion

namespace hyperion::v2 {

using renderer::BlendMode;

class Material : public EngineComponentBase<STUB_CLASS(Material)>
{
public:
    static constexpr UInt max_parameters = 32u;
    static constexpr UInt max_textures = 32u;

    static constexpr UInt max_textures_to_set = MathUtil::Min(
        max_textures,
        MaterialShaderData::max_bound_textures,
        HYP_FEATURES_BINDLESS_TEXTURES
            ? DescriptorSet::max_bindless_resources
            : DescriptorSet::max_material_texture_samplers
    );

    using TextureKeyType = UInt64;

    enum TextureKey : TextureKeyType
    {
        MATERIAL_TEXTURE_NONE = 0,

        MATERIAL_TEXTURE_ALBEDO_MAP    = 1 << 0,
        MATERIAL_TEXTURE_NORMAL_MAP    = 1 << 1,
        MATERIAL_TEXTURE_AO_MAP        = 1 << 2,
        MATERIAL_TEXTURE_PARALLAX_MAP  = 1 << 3,
        MATERIAL_TEXTURE_METALNESS_MAP = 1 << 4,
        MATERIAL_TEXTURE_ROUGHNESS_MAP = 1 << 5,
        MATERIAL_TEXTURE_SKYBOX_MAP    = 1 << 6,
        MATERIAL_TEXTURE_COLOR_MAP     = 1 << 7,
        MATERIAL_TEXTURE_POSITION_MAP  = 1 << 8,
        MATERIAL_TEXTURE_DATA_MAP      = 1 << 9,
        MATERIAL_TEXTURE_SSAO_MAP      = 1 << 10,
        MATERIAL_TEXTURE_TANGENT_MAP   = 1 << 11,
        MATERIAL_TEXTURE_BITANGENT_MAP = 1 << 12,
        MATERIAL_TEXTURE_DEPTH_MAP     = 1 << 13,

        // terrain

        MATERIAL_TEXTURE_SPLAT_MAP = 1 << 14,

        MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP      = 1 << 15,
        MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP     = 1 << 16,
        MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP         = 1 << 17,
        MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP   = 1 << 18,

        MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP    = 1 << 19,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP   = 1 << 20,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP       = 1 << 21,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP = 1 << 22,

        MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP    = 1 << 23,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP   = 1 << 24,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP       = 1 << 25,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP = 1 << 26
    };

    struct Parameter
    {
        union
        {
            float float_values[4];
            int int_values[4];
        } values;

        enum Type : UInt32
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

        Parameter() : type(MATERIAL_PARAMETER_TYPE_NONE) {}

        template <SizeType Size>
        explicit Parameter(FixedArray<Float, Size> &&v)
            : Parameter(v.Data(), Size)
        {
        }
        
        explicit Parameter(const Float *v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_FLOAT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.float_values, v, count * sizeof(Float));
        }
        
        Parameter(Float value)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT)
        {
            std::memcpy(values.float_values, &value, sizeof(Float));
        }

        Parameter(const Vector2 &xy)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT2)
        {
            std::memcpy(values.float_values, &xy.values, 2 * sizeof(Float));
        }

        Parameter(const Vector3 &xyz)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT3)
        {
            std::memcpy(values.float_values, &xyz.values, 3 * sizeof(Float));
        }

        Parameter(const Vector4 &xyzw)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT4)
        {
            std::memcpy(values.float_values, &xyzw.values, 4 * sizeof(Float));
        }

        Parameter(const Color &color)
            : Parameter(Vector4(color.GetRed(), color.GetGreen(), color.GetBlue(), color.GetAlpha()))
        {
        }

        template <SizeType Size>
        explicit Parameter(FixedArray<Int32, Size> &&v)
            : Parameter(v.Data(), Size)
        {
        }
        
        explicit Parameter(const Int32 *v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_INT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.int_values, v, count * sizeof(Int32));
        }

        explicit Parameter(const Parameter &other)
            : type(other.type)
        {
            std::memcpy(&values, &other.values, sizeof(values));
        }

        Parameter &operator=(const Parameter &other)
        {
            type = other.type;
            std::memcpy(&values, &other.values, sizeof(values));

            return *this;
        }

        ~Parameter() = default;

        bool IsIntType() const
            { return type >= MATERIAL_PARAMETER_TYPE_INT && type <= MATERIAL_PARAMETER_TYPE_INT4; }

        bool IsFloatType() const
            { return type >= MATERIAL_PARAMETER_TYPE_FLOAT && type <= MATERIAL_PARAMETER_TYPE_FLOAT4; }

        UInt Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return 0u;
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT) {
                return UInt(type - MATERIAL_PARAMETER_TYPE_INT) + 1;
            }

            return UInt(type);
        }

        void Copy(unsigned char dst[4]) const
        {
            std::memcpy(dst, &values, Size());
        }

        bool operator==(const Parameter &other) const
        {
            return std::memcmp(&values, &other.values, sizeof(values)) == 0;
        }

        HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(int(type));

            if (IsIntType()) {
                hc.Add(values.int_values[0]);
                hc.Add(values.int_values[1]);
                hc.Add(values.int_values[2]);
                hc.Add(values.int_values[3]);
            } else if (IsFloatType()) {
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
    
    enum MaterialKey : UInt64
    {
        MATERIAL_KEY_NONE = 0,

        // basic
        MATERIAL_KEY_ALBEDO                 = 1 << 0,
        MATERIAL_KEY_METALNESS              = 1 << 1,
        MATERIAL_KEY_ROUGHNESS              = 1 << 2,
        MATERIAL_KEY_TRANSMISSION           = 1 << 3,
        MATERIAL_KEY_EMISSIVE               = 1 << 4,  // UNUSED
        MATERIAL_KEY_SPECULAR               = 1 << 5,  // UNUSED
        MATERIAL_KEY_SPECULAR_TINT          = 1 << 6,  // UNUSED
        MATERIAL_KEY_ANISOTROPIC            = 1 << 7,  // UNUSED
        MATERIAL_KEY_SHEEN                  = 1 << 8,  // UNUSED
        MATERIAL_KEY_SHEEN_TINT             = 1 << 9,  // UNUSED
        MATERIAL_KEY_CLEARCOAT              = 1 << 10,  // UNUSED
        MATERIAL_KEY_CLEARCOAT_GLOSS        = 1 << 11, // UNUSED
        MATERIAL_KEY_SUBSURFACE             = 1 << 12,  // UNUSED
        MATERIAL_KEY_NORMAL_MAP_INTENSITY   = 1 << 13,
        MATERIAL_KEY_UV_SCALE               = 1 << 14,
        MATERIAL_KEY_PARALLAX_HEIGHT        = 1 << 15,
        MATERIAL_KEY_ALPHA_THRESHOLD        = 1 << 16,
        MATERIAL_KEY_RESERVED2              = 1 << 17,

        // terrain
        MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT = 1 << 18,
        MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT = 1 << 19,
        MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT = 1 << 20,
        MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT = 1 << 21
    };

    using ParameterTable = EnumOptions<MaterialKey, Parameter, max_parameters>;
    using TextureSet = EnumOptions<TextureKey, Handle<Texture>, max_textures>;

    static ParameterTable DefaultParameters();

    Material();
    Material(
        Name name,
        Bucket bucket = Bucket::BUCKET_OPAQUE
    );
    Material(
        Name name,
        const MaterialAttributes &attributes,
        const ParameterTable &parameters,
        const TextureSet &textures
    );
    Material(const Material &other) = delete;
    Material &operator=(const Material &other) = delete;
    ~Material();

    ShaderDataState GetShaderDataState() const
        { return m_shader_data_state; }

    void SetShaderDataState(ShaderDataState state)
        { m_shader_data_state = state; }

    ParameterTable &GetParameters()
        { return m_parameters; }

    const ParameterTable &GetParameters() const
        { return m_parameters; }

    const Parameter &GetParameter(MaterialKey key) const
        { return m_parameters.Get(key); }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
        { return m_parameters.Get(key).values.float_values[0]; }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, int>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
        { return m_parameters.Get(key).values.int_values[0]; }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<decltype(T::values[0])>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
    {
        static_assert(sizeof(T::values) <= sizeof(Parameter::values));

        T result;
        std::memcpy(&result.values[0], &m_parameters.Get(key).values.float_values[0], sizeof(float) * std::size(result.values));
        return result;
    }

    /*! \brief Set a parameter on this material with the given key and value
     * @param key The key of the parameter to be set
     * @param value The value of the parameter to be set
     */
    void SetParameter(MaterialKey key, const Parameter &value);

    /*! \brief Set all parameters back to their default values. */
    void ResetParameters();

    /*! \brief Sets the texture with the given key on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material is initialized.
     * @param key The texture slot to set the texture on
     * @param texture A Texture resource
     */
    void SetTexture(TextureKey key, Handle<Texture> &&texture);

    /*! \brief Sets the texture with the given key on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material is initialized.
     * @param key The texture slot to set the texture on
     * @param texture A Texture resource
     */
    void SetTexture(TextureKey key, const Handle<Texture> &texture);

    /*! \brief Sets the texture at the given index on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material is initialized.
     * @param index The index to set the texture in
     * @param texture A Texture resource
     */
    void SetTextureAtIndex(UInt index, const Handle<Texture> &texture);

    TextureSet &GetTextures()
        { return m_textures; }

    const TextureSet &GetTextures() const
        { return m_textures; }

    /*! \brief Return a pointer to a Texture set on this Material by the given
     * texture key. If no Texture was set, nullptr is returned.
     * @param key The key of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    const Handle<Texture> &GetTexture(TextureKey key) const;

    /*! \brief Return a pointer to a Texture set on this Material by the given
     * index. If no Texture was set, nullptr is returned.
     * @param index The index of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    const Handle<Texture> &GetTextureAtIndex(UInt index) const;

    Bucket GetBucket() const
        { return m_render_attributes.bucket; }

    void SetBucket(Bucket bucket)
        { m_render_attributes.bucket = bucket; }

    bool IsAlphaBlended() const
        { return m_render_attributes.blend_mode != BlendMode::NONE; }

    void SetIsAlphaBlended(bool is_alpha_blended, BlendMode blend_mode = BlendMode::NORMAL)
    {
        if (is_alpha_blended) {
            m_render_attributes.blend_mode = blend_mode;
        } else {
            m_render_attributes.blend_mode = BlendMode::NONE;
        }
    }

    BlendMode GetBlendMode() const
        { return m_render_attributes.blend_mode; }

    void SetBlendMode(BlendMode blend_mode)
        { m_render_attributes.blend_mode = blend_mode; }

    bool IsDepthWriteEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE); }

    void SetIsDepthWriteEnabled(bool is_depth_write_enabled)
    {
        if (is_depth_write_enabled) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE;
        }
    }

    bool IsDepthTestEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST); }

    void SetIsDepthTestEnabled(bool is_depth_test_enabled)
    {
        if (is_depth_test_enabled) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
        }
    }

    FaceCullMode GetFaceCullMode() const
        { return m_render_attributes.cull_faces; }

    void SetFaceCullMode(FaceCullMode cull_mode)
        { m_render_attributes.cull_faces = cull_mode; }

    MaterialAttributes &GetRenderAttributes()
        { return m_render_attributes; }

    const MaterialAttributes &GetRenderAttributes() const
        { return m_render_attributes; }

    bool IsStatic() const
        { return !m_is_dynamic; }

    bool IsDynamic() const
        { return m_is_dynamic; }

    void Init();
    void Update();

    Handle<Material> Clone() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_parameters.GetHashCode());
        hc.Add(m_textures.GetHashCode());
        hc.Add(m_render_attributes.GetHashCode());

        return hc;
    }

private:
    void EnqueueRenderUpdates();
    void EnqueueTextureUpdate(TextureKey key);
    void EnqueueDescriptorSetCreate();
    void EnqueueDescriptorSetDestroy();

    ParameterTable m_parameters;
    TextureSet m_textures;

    MaterialAttributes m_render_attributes;

    bool m_is_dynamic;

    MaterialShaderData m_shader_data;
    mutable ShaderDataState m_shader_data_state;

    FixedArray<DescriptorSet *, max_frames_in_flight> m_descriptor_sets;
};

class MaterialGroup : public EngineComponentBase<STUB_CLASS(MaterialGroup)>
{
public:
    MaterialGroup();
    MaterialGroup(const MaterialGroup &other) = delete;
    MaterialGroup &operator=(const MaterialGroup &other) = delete;
    ~MaterialGroup();

    void Init();
    void Add(const String &name, Handle<Material> &&material);
    bool Remove(const String &name);

    Handle<Material> &Get(const String &name)
        { return m_materials[name]; }

    const Handle<Material> &Get(const String &name) const
        { return m_materials.At(name); }

    bool Has(const String &name) const
        { return m_materials.Contains(name); }

private:
    HashMap<String, Handle<Material>> m_materials;
};

class MaterialCache
{
public:
    void Add(const Handle<Material> &material);

    Handle<Material> GetOrCreate(
        const MaterialAttributes &attributes = { },
        const Material::ParameterTable &parameters = Material::DefaultParameters(),
        const Material::TextureSet &textures = { }
    );

private:
    FlatMap<HashCode::ValueType, WeakHandle<Material>> m_map;
    std::mutex m_mutex;
};

} // namespace hyperion::v2

#endif
