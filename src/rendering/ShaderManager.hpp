#ifndef HYPERION_V2_SHADER_MANAGER_H
#define HYPERION_V2_SHADER_MANAGER_H

#include "Shader.hpp"
#include <core/lib/FlatMap.hpp>

namespace hyperion::v2 {


class ShaderManager {
public:
    using Key = ShaderKey;

    void SetShader(const std::string &name, Ref<Shader> &&shader)
    {
        SetShader(Key::CUSTOM, name, std::move(shader));
    }

    void SetShader(Key key, Ref<Shader> &&shader)
    {
        SetShader(key, "", std::move(shader));
    }

    Ref<Shader> &GetShader(const std::string &name)
        { return GetShader(Key::CUSTOM, name); }

    Ref<Shader> &GetShader(Key key)
        { return GetShader(key, ""); }

private:
    Ref<Shader> &GetShader(Key key, const std::string &name)
        { return m_shaders[ShaderMapKey{key, name}]; }

    void SetShader(Key key, const std::string &name, Ref<Shader> &&shader)
    {
        m_shaders[ShaderMapKey{key, name}] = std::move(shader);
    }

    FlatMap<ShaderMapKey, Ref<Shader>> m_shaders;
};

} // namespace hyperion::v2

#endif