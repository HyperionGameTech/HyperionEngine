#ifndef SHADER_H
#define SHADER_H

#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../math/matrix4.h"
#include "../math/transform.h"
#include "../asset/loadable.h"
#include "../hash_code.h"
#include "../util.h"
#include "material.h"
#include "camera/camera.h"

#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <string>
#include <cstring>

namespace hyperion {
class Texture;

class ShaderProperties {
public:
    struct ShaderProperty {
        std::string raw_value;

        union Value {
            float float_value;
            int int_value;
            bool bool_value;
        } value;

        enum ShaderPropertyType {
            SHADER_PROPERTY_UNSET = 0,
            SHADER_PROPERTY_STRING,
            SHADER_PROPERTY_FLOAT,
            SHADER_PROPERTY_INT,
            SHADER_PROPERTY_BOOL
        } type;

        inline bool operator==(const ShaderProperty &other) const
        {
            return type == other.type &&
                raw_value == other.raw_value;
        }

        inline bool IsTruthy() const
        {
            switch (type) {
            case SHADER_PROPERTY_UNSET:
                return false;
            case SHADER_PROPERTY_STRING:
                return true;
            case SHADER_PROPERTY_INT:
                return value.int_value != 0;
            case SHADER_PROPERTY_FLOAT:
                return value.float_value != 0.0;
            case SHADER_PROPERTY_BOOL:
                return !!value.bool_value;
            default:
                return false;
            }
        }

        inline HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(int(type));
            hc.Add(raw_value);

            return hc;
        }
    };

    ShaderProperties() {}
    ShaderProperties(const ShaderProperties &other)
        : m_properties(other.m_properties)
    {
    }

    std::unordered_map<std::string, ShaderProperty> m_properties;

    inline bool operator==(const ShaderProperties &other) const
    {
        return m_properties == other.m_properties;
    }

    inline bool HasValue(const std::string &key) const
    {
        auto it = m_properties.find(key);

        return it != m_properties.end();
    }

    inline ShaderProperty GetValue(const std::string &key) const
    {
        ShaderProperty result;
        result.value.int_value = 0;
        result.type = ShaderProperty::SHADER_PROPERTY_UNSET;

        auto it = m_properties.find(key);

        if (it != m_properties.end()) {
            result = it->second;
        }

        return result;
    }

    inline ShaderProperties &Define(const std::string &key, const std::string &value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_STRING;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Define(const std::string &key, int value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = std::to_string(value);
        shader_property.value.int_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_INT;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Define(const std::string &key, float value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = std::to_string(value);
        shader_property.value.float_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_FLOAT;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Define(const std::string &key, bool value)
    {
        ShaderProperty shader_property;
        shader_property.raw_value = std::to_string(value);
        shader_property.value.bool_value = value;
        shader_property.type = ShaderProperty::ShaderPropertyType::SHADER_PROPERTY_BOOL;

        m_properties[key] = shader_property;

        return *this;
    }

    inline ShaderProperties &Merge(const ShaderProperties &other)
    {
        for (auto &it : other.m_properties) {
            m_properties[it.first] = it.second;
        }

        return *this;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_properties) {
            hc.Add(it.first);
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }
};

class Shader {
    friend class Renderer;
public:
    Shader(const ShaderProperties &properties);
    Shader(const ShaderProperties &properties,
        const std::string &vscode,
        const std::string &fscode);
    Shader(const Shader &other) = delete;
    Shader &operator=(const Shader &other) = delete;
    virtual ~Shader();

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

    inline int GetId() const { return progid; }

    inline ShaderProperties &GetProperties() { return m_properties; }
    inline const ShaderProperties &GetProperties() const { return m_properties; }
    inline void SetProperties(const ShaderProperties &properties) { m_properties = properties; }

    inline MaterialFaceCull SetOverrideCullMode() const { return m_override_cull; }
    inline void SetOverrideCullMode(MaterialFaceCull cull_mode) { m_override_cull = cull_mode; }

    inline void SetUniform(const std::string &name, float value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, int value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Texture *value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector2 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector3 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Vector4 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }
    inline void SetUniform(const std::string &name, const Matrix4 &value) { uniforms[name] = Uniform(value); uniform_changed = true; }

    void Use();
    void End();

protected:
    ShaderProperties m_properties;
    MaterialFaceCull m_override_cull;

    enum SubShaderType {
        SUBSHADER_NONE = 0x00,
        SUBSHADER_FRAGMENT = 0x8B30,
        SUBSHADER_VERTEX = 0x8B31,
        SUBSHADER_GEOMETRY = 0x8DD9,
        SUBSHADER_COMPUTE = 0x91B9
    };

    struct SubShader {
        SubShaderType type;
        int id;
        std::string code;
        std::string processed_code;
        std::string path;

        SubShader()
            : type(SubShaderType::SUBSHADER_NONE),
              id(0)
        {
        }

        SubShader(SubShaderType type, const std::string &code)
            : type(type),
              id(0),
              code(code)
        {
        }

        SubShader(const SubShader &other)
            : type(other.type),
              id(other.id),
              code(other.code),
              processed_code(other.processed_code),
              path(other.path)
        {
        }
    };

    void AddSubShader(SubShaderType type,
        const std::string &code,
        const ShaderProperties &properties,
        const std::string &path);
    void ReprocessSubShader(SubShader &sub_shader, const ShaderProperties &properties);

    // TODO: add better resetting of uniforms
    // track all uniforms that were set and unset their values.
    // still, we should be able to keep base uniforms set during the constructor.
    // two different paths..? a flag on the uniform?
    void ResetUniforms();

private:
    bool is_uploaded, is_created, uniform_changed;
    unsigned int progid;

    size_t m_previous_properties_hash_code;

    void CreateGpuData();
    void UploadGpuData();
    void DestroyGpuData();
    bool ShaderPropertiesChanged() const;
    // void OutputShaderError(int id);

    void ApplyUniforms();

    struct Uniform {
        enum UniformType {
            Uniform_None = -1,
            Uniform_Float,
            Uniform_Int,
            Uniform_Vector2,
            Uniform_Vector3,
            Uniform_Vector4,
            Uniform_Matrix4,
            Uniform_Texture2D,
            Uniform_Texture3D,
            Uniform_TextureCube
        } type;

        std::array<float, 16> data;

        Uniform()
        {
            data = { 0.0f };
            type = Uniform_None;
        }

        Uniform(float value)
        {
            data[0] = value;
            type = Uniform_Float;
        }

        Uniform(int value)
        {
            data[0] = (float)value;
            type = Uniform_Int;
        }

        Uniform(const Vector2 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            type = Uniform_Vector2;
        }

        Uniform(const Vector3 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            data[2] = value.z;
            type = Uniform_Vector3;
        }

        Uniform(const Vector4 &value)
        {
            data[0] = value.x;
            data[1] = value.y;
            data[2] = value.z;
            data[3] = value.w;
            type = Uniform_Vector4;
        }

        Uniform(const Matrix4 &value)
        {
            std::memcpy(&data[0], &value.values[0], value.values.size() * sizeof(float));
            type = Uniform_Matrix4;
        }

        Uniform(const Texture *texture)
        {
            ex_assert(texture != nullptr);

            data[0] = texture->GetId();
            // texture->GetTextureType() should start at 0 and map to the correct uniform texture type
            type = UniformType(int(Uniform_Texture2D) + int(texture->GetTextureType()));
        }

        Uniform &operator=(const Uniform &other)
        {
            type = other.type;
            data = other.data;
            return *this;
        }
    };

    std::map<SubShaderType, SubShader> subshaders;
    std::map<std::string, Uniform> uniforms;
};

} // namespace hyperion

#endif
