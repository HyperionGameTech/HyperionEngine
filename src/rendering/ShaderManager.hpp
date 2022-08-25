#ifndef HYPERION_V2_SHADER_MANAGER_H
#define HYPERION_V2_SHADER_MANAGER_H

#include "Shader.hpp"
#include <core/lib/FlatMap.hpp>

namespace hyperion::v2 {


class ShaderManager
{
public:
    using Key = ShaderKey;

    void SetShader(const std::string &name, Handle<Shader> &&shader)
    {
        SetShader(Key::CUSTOM, name, std::move(shader));
    }

    void SetShader(Key key, Handle<Shader> &&shader)
    {
        SetShader(key, "", std::move(shader));
    }

    Handle<Shader> &GetShader(const std::string &name)
        { return GetShader(Key::CUSTOM, name); }

    Handle<Shader> &GetShader(Key key)
        { return GetShader(key, ""); }

private:
    Handle<Shader> &GetShader(Key key, const std::string &name)
        { return m_shaders[ShaderMapKey{key, name}]; }

    void SetShader(Key key, const std::string &name, Handle<Shader> &&shader)
    {
        m_shaders[ShaderMapKey{key, name}] = std::move(shader);
    }

    FlatMap<ShaderMapKey, Handle<Shader>> m_shaders;
};

} // namespace hyperion::v2

#endif