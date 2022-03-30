#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include "texture.h"
#include "shader.h"

#include <util/enum_options.h>
#include <hash_code.h>

namespace hyperion::v2 {

class TextureSet {
public:
    static constexpr size_t max_textures = 32;

    enum TextureKey : uint64_t {
        MATERIAL_TEXTURE_NONE = 0,

        MATERIAL_TEXTURE_DIFFUSE_MAP = 1 << 0,
        MATERIAL_TEXTURE_NORMAL_MAP = 1 << 1,
        MATERIAL_TEXTURE_AO_MAP = 1 << 2,
        MATERIAL_TEXTURE_PARALLAX_MAP = 1 << 3,
        MATERIAL_TEXTURE_METALNESS_MAP = 1 << 4,
        MATERIAL_TEXTURE_ROUGHNESS_MAP = 1 << 5,
        MATERIAL_TEXTURE_SKYBOX_MAP = 1 << 6,
        MATERIAL_TEXTURE_COLOR_MAP = 1 << 7,
        MATERIAL_TEXTURE_POSITION_MAP = 1 << 8,
        MATERIAL_TEXTURE_DATA_MAP = 1 << 9,
        MATERIAL_TEXTURE_SSAO_MAP = 1 << 10,
        MATERIAL_TEXTURE_TANGENT_MAP = 1 << 11,
        MATERIAL_TEXTURE_BITANGENT_MAP = 1 << 12,
        MATERIAL_TEXTURE_DEPTH_MAP = 1 << 13,

        // terrain

        MATERIAL_TEXTURE_SPLAT_MAP = 1 << 14,

        MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP = 1 << 15,
        MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP = 1 << 16,
        MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP = 1 << 17,
        MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP = 1 << 18,

        MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP = 1 << 19,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP = 1 << 20,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP = 1 << 21,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP = 1 << 22,

        MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP = 1 << 23,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP = 1 << 24,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP = 1 << 25,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP = 1 << 26
    };


    EnumOptions<TextureKey, Texture::ID, max_textures> m_textures;
};

class Material : public EngineComponentBase<STUB_CLASS(Material)> {
public:
    static constexpr size_t max_parameters = 32;

    struct Parameter {
        union {
            float float_values[4];
            int int_values[4];
        } values;

        enum Type {
            MATERIAL_PARAMETER_TYPE_NONE,
            MATERIAL_PARAMETER_TYPE_FLOAT_1,
            MATERIAL_PARAMETER_TYPE_FLOAT_2,
            MATERIAL_PARAMETER_TYPE_FLOAT_3,
            MATERIAL_PARAMETER_TYPE_FLOAT_4,
            MATERIAL_PARAMETER_TYPE_INT_1,
            MATERIAL_PARAMETER_TYPE_INT_2,
            MATERIAL_PARAMETER_TYPE_INT_3,
            MATERIAL_PARAMETER_TYPE_INT_4
        } type;

        Parameter() : type(MATERIAL_PARAMETER_TYPE_NONE) {}

        template <size_t Size>
        explicit Parameter(std::array<float, Size> &&v)
            : type(Type(MATERIAL_PARAMETER_TYPE_FLOAT_1 + (Size - 1)))
        {
            static_assert(Size >= 1 && Size <= 4);

            std::memcpy(values.float_values, v, Size * sizeof(float));
        }
        
        explicit Parameter(float value)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT_1)
        {
            std::memcpy(values.float_values, &value, sizeof(float));
        }

        explicit Parameter(const Vector2 &xy)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT_2)
        {
            std::memcpy(values.float_values, &xy.values, 2 * sizeof(float));
        }

        explicit Parameter(const Vector3 &xyz)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT_3)
        {
            std::memcpy(values.float_values, &xyz.values, 3 * sizeof(float));
        }

        explicit Parameter(const Vector4 &xyzw)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT_4)
        {
            std::memcpy(values.float_values, &xyzw.values, 4 * sizeof(float));
        }

        template <size_t Size>
        explicit Parameter(std::array<int, Size> &&v)
            : type(Type(MATERIAL_PARAMETER_TYPE_INT_1 + (Size - 1)))
        {
            static_assert(Size >= 1 && Size <= 4);

            std::memcpy(values.int_values, v.data(), Size * sizeof(int));
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
            { return type >= MATERIAL_PARAMETER_TYPE_INT_1 && type <= MATERIAL_PARAMETER_TYPE_INT_4; }

        inline bool IsFloatType() const
            { return type >= MATERIAL_PARAMETER_TYPE_FLOAT_1 && type <= MATERIAL_PARAMETER_TYPE_FLOAT_4; }

        inline uint8_t Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return uint8_t(0);
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT_1) {
                return uint8_t(type - MATERIAL_PARAMETER_TYPE_INT_1) + 1;
            }

            return uint8_t(type);
        }

        inline void Copy(unsigned char dst[4]) const
        {
            std::memcpy(dst, &values, Size());
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
        MATERIAL_KEY_SUBSURFACE             = 1 << 3,
        MATERIAL_KEY_SPECULAR               = 1 << 4,
        MATERIAL_KEY_SPECULAR_TINT          = 1 << 5,
        MATERIAL_KEY_ANISOTROPIC            = 1 << 6,
        MATERIAL_KEY_SHEEN                  = 1 << 7,
        MATERIAL_KEY_SHEEN_TINT             = 1 << 8,
        MATERIAL_KEY_CLEARCOAT              = 1 << 9,
        MATERIAL_KEY_CLEARCOAT_GLOSS        = 1 << 10,
        MATERIAL_KEY_EMISSIVENESS           = 1 << 11,
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

    Material();
    Material(const Material &other);
    Material &operator=(const Material &other);
    Material(Material &&other);
    Material &operator=(Material &&other);
    ~Material();

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
        T result;
        std::memcpy(&result.values[0], &m_parameters.Get(key).values.float_values[0], sizeof(float) * std::size(result.values));
        return result;
    }

    void SetParameter(MaterialKey key, const Parameter &value);
    void SetTexture(TextureSet::TextureKey key, Texture::ID id);

    void Create(Engine *engine, MaterialShaderData *out);
    void Destroy(Engine *engine);

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_parameters.GetHashCode());

        return hc;
    }

private:
    ParameterTable m_parameters;
    State m_state;
};

} // namespace hyperion::v2

#endif