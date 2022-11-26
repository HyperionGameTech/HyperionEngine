#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include <core/Base.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

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

struct SubShader
{
    ShaderModule::Type type;
    ShaderObject spirv;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(spirv);

        return hc;
    }
};

class Shader
    : public EngineComponentBase<STUB_CLASS(Shader)>,
      public RenderResource
{
public:
    Shader(const std::vector<SubShader> &sub_shaders);
    Shader(const CompiledShader &compiled_shader);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    ShaderProgram *GetShaderProgram() const { return m_shader_program.get(); }

    auto &GetSubShaders() { return m_sub_shaders; }
    const auto &GetSubShaders() const { return m_sub_shaders; }

    void Init();

private:
    std::unique_ptr<ShaderProgram> m_shader_program;
    std::vector<SubShader> m_sub_shaders;
};

enum class ShaderKey
{
    BASIC_FORWARD,
    BASIC_FORWARD_SKINNED,
    BASIC_VEGETATION,
    BASIC_SKYBOX,
    BASIC_UI,
    STENCIL_OUTLINE,
    DEBUG_AABB,
    TERRAIN,
    CUSTOM
};

using ShaderMapKey = KeyValuePair<ShaderKey, String>;

} // namespace hyperion::v2

HYP_DEF_STL_HASH(hyperion::v2::ShaderMapKey);

#endif // !HYPERION_V2_SHADER_H

