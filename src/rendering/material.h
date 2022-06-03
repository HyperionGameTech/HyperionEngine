#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include "texture.h"
#include "shader.h"

#include <util/enum_options.h>
#include <hash_code.h>

namespace hyperion::v2 {

class Material : public EngineComponentBase<STUB_CLASS(Material)> {
public:
    static constexpr size_t max_parameters = 32;
    static constexpr size_t max_textures = 32;

    enum TextureKey : uint64_t {
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

    struct Parameter {
        union {
            float float_values[4];
            int   int_values[4];
            void *ptr;
        } values;

        enum Type {
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

        template <size_t Size>
        explicit Parameter(std::array<float, Size> &&v)
            : Parameter(v.data(), v.size())
        {
        }
        
        explicit Parameter(const float *v, size_t count)
            : type(Type(MATERIAL_PARAMETER_TYPE_FLOAT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.float_values, v, count * sizeof(float));
        }
        
        Parameter(float value)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT)
        {
            std::memcpy(values.float_values, &value, sizeof(float));
        }

        Parameter(const Vector2 &xy)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT2)
        {
            std::memcpy(values.float_values, &xy.values, 2 * sizeof(float));
        }

        Parameter(const Vector3 &xyz)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT3)
        {
            std::memcpy(values.float_values, &xyz.values, 3 * sizeof(float));
        }

        Parameter(const Vector4 &xyzw)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT4)
        {
            std::memcpy(values.float_values, &xyzw.values, 4 * sizeof(float));
        }

        template <size_t Size>
        explicit Parameter(std::array<int, Size> &&v)
            : Parameter(v.data(), v.size())
        {
        }
        
        explicit Parameter(const int *v, size_t count)
            : type(Type(MATERIAL_PARAMETER_TYPE_INT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.int_values, v, count * sizeof(int));
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

        inline bool IsIntType() const
            { return type >= MATERIAL_PARAMETER_TYPE_INT && type <= MATERIAL_PARAMETER_TYPE_INT4; }

        inline bool IsFloatType() const
            { return type >= MATERIAL_PARAMETER_TYPE_FLOAT && type <= MATERIAL_PARAMETER_TYPE_FLOAT4; }

        inline uint8_t Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return uint8_t(0);
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT) {
                return uint8_t(type - MATERIAL_PARAMETER_TYPE_INT) + 1;
            }

            return uint8_t(type);
        }

        inline void Copy(unsigned char dst[4]) const
        {
            std::memcpy(dst, &values, Size());
        }

        inline bool operator==(const Parameter &other) const
        {
            return values.ptr == other.values.ptr
                || !std::memcmp(&values, &other.values, sizeof(values));
        }

        inline HashCode GetHashCode() const
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

    enum State {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };
    
    enum MaterialKey : uint64_t {
        MATERIAL_KEY_NONE = 0,

        // basic
        MATERIAL_KEY_ALBEDO                 = 1 << 0,
        MATERIAL_KEY_METALNESS              = 1 << 1,
        MATERIAL_KEY_ROUGHNESS              = 1 << 2,
        MATERIAL_KEY_SUBSURFACE             = 1 << 3,  // UNUSED
        MATERIAL_KEY_SPECULAR               = 1 << 4,  // UNUSED
        MATERIAL_KEY_SPECULAR_TINT          = 1 << 5,  // UNUSED
        MATERIAL_KEY_ANISOTROPIC            = 1 << 6,  // UNUSED
        MATERIAL_KEY_SHEEN                  = 1 << 7,  // UNUSED
        MATERIAL_KEY_SHEEN_TINT             = 1 << 8,  // UNUSED
        MATERIAL_KEY_CLEARCOAT              = 1 << 9,  // UNUSED
        MATERIAL_KEY_CLEARCOAT_GLOSS        = 1 << 10, // UNUSED
        MATERIAL_KEY_EMISSIVENESS           = 1 << 11, // UNUSED
        MATERIAL_KEY_FLIP_UV                = 1 << 12,
        MATERIAL_KEY_UV_SCALE               = 1 << 13,
        MATERIAL_KEY_PARALLAX_HEIGHT        = 1 << 14,
        MATERIAL_KEY_RESERVED1              = 1 << 15,
        MATERIAL_KEY_RESERVED2              = 1 << 16,
        MATERIAL_KEY_RESERVED3              = 1 << 17,

        // terrain
        MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT = 1 << 18,
        MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT = 1 << 19,
        MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT = 1 << 20,
        MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT = 1 << 21
    };

    using ParameterTable = EnumOptions<MaterialKey, Parameter, max_parameters>;
    using TextureSet     = EnumOptions<TextureKey, Ref<Texture>, max_textures>;

    Material(const char *tag = "");
    Material(const Material &other) = delete;
    Material &operator=(const Material &other) = delete;
    ~Material();

    ShaderDataState GetShaderDataState() const     { return m_shader_data_state; }
    void SetShaderDataState(ShaderDataState state) { m_shader_data_state = state; }

    inline const Parameter &GetParameter(MaterialKey key) const
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

    /* \brief Set a parameter on this material with the given key and value
     * @param key The key of the parameter to be set
     * @param value The value of the parameter to be set
     */
    void SetParameter(MaterialKey key, const Parameter &value);

    /* \brief Set all parameters back to their default values. */
    void ResetParameters();

    /* \brief Sets the texture with the given key on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material's init callback is fired off.
     * @param key The texture slot to set the texture on
     * @param texture A Texture resource
     */
    void SetTexture(TextureKey key, Ref<Texture> &&texture);

    /* \brief Return a pointer to a Texture set on this Material by the given
     * texture key. If no Texture was set, nullptr is returned.
     * @param key The key of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    Texture *GetTexture(TextureKey key) const;

    void Init(Engine *engine);
    void Update(Engine *engine);

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_parameters.GetHashCode());

        return hc;
    }

private:
    static constexpr uint max_textures_to_set = MathUtil::Min(
        max_textures,
        MaterialShaderData::max_bound_textures,
        HYP_FEATURES_BINDLESS_TEXTURES
            ? DescriptorSet::max_bindless_resources
            : DescriptorSet::max_material_texture_samplers
    );

    void EnqueueRenderUpdates();
    void EnqueueTextureUpdate(TextureKey key);
    void EnqueueDescriptorSetCreation();

    char *m_tag;

    ParameterTable          m_parameters;
    TextureSet              m_textures;

    MaterialShaderData      m_shader_data;
    mutable ShaderDataState m_shader_data_state;
};

class MaterialGroup {
public:
    MaterialGroup();
    MaterialGroup(const MaterialGroup &other) = delete;
    MaterialGroup &operator=(const MaterialGroup &other) = delete;
    ~MaterialGroup();

    void Add(const std::string &name, Ref<Material> &&material)
        { m_materials[name] = std::move(material); }

    bool Remove(const std::string &name)
    {
        const auto it = m_materials.find(name);

        if (it != m_materials.end()) {
            m_materials.erase(it);

            return true;
        }

        return false;
    }

    Ref<Material> &Get(const std::string &name)
        { return m_materials[name]; }

    const Ref<Material> &Get(const std::string &name) const
        { return m_materials.at(name); }

    bool Has(const std::string &name) const
        { return m_materials.find(name) != m_materials.end(); }

private:
    std::unordered_map<std::string, Ref<Material>> m_materials;
};

} // namespace hyperion::v2

#endif
