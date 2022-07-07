#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "Base.hpp"
#include "Bindless.hpp"
#include "ShaderGlobals.hpp"

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <math/Transform.hpp>

#include <HashCode.hpp>

#include <memory>
#include <utility>
#include <string>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;

class Engine;

struct SubShader {
    ShaderModule::Type type;
    ShaderObject       spirv;
};

class Shader
    : public EngineComponentBase<STUB_CLASS(Shader)>,
      public RenderObject
{
public:
    Shader(const std::vector<SubShader> &sub_shaders);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    ShaderProgram *GetShaderProgram() const { return m_shader_program.get(); }

    void Init(Engine *engine);

private:
    std::unique_ptr<ShaderProgram> m_shader_program;
    std::vector<SubShader>         m_sub_shaders;
};

enum class ShaderKey {
    BASIC_FORWARD,
    BASIC_VEGETATION,
    BASIC_SKYBOX,
    STENCIL_OUTLINE,
    DEBUG_AABB,
    CUSTOM
};

struct ShaderMapKey {
    ShaderKey   key;
    std::string name;

    bool operator==(const ShaderMapKey &other) const
        { return key == other.key && name == other.name; }

    bool operator<(const ShaderMapKey &other) const
        { return std::tie(key, name) < std::tie(other.key, other.name); }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(int(key));
        hc.Add(name);

        return hc;
    }
};

} // namespace hyperion::v2

HYP_DEF_STL_HASH(hyperion::v2::ShaderMapKey);

#endif // !HYPERION_V2_SHADER_H

