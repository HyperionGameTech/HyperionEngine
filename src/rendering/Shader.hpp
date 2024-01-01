#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include <core/Base.hpp>
#include <core/Name.hpp>

#include <rendering/Bindless.hpp>
#include <rendering/backend/RenderObject.hpp>
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
using renderer::ShaderModuleType;

class Engine;

struct SubShader
{
    ShaderModuleType    type;
    ShaderObject        spirv;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(spirv);

        return hc;
    }
};

class Shader
    : public BasicObject<STUB_CLASS(Shader)>
{
public:
    Shader();
    Shader(const CompiledShader &compiled_shader);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    const CompiledShader &GetCompiledShader() const
        { return m_compiled_shader; }

    void SetCompiledShader(const CompiledShader &compiled_shader)
        { m_compiled_shader = compiled_shader; }

    void SetCompiledShader(CompiledShader &&compiled_shader)
        { m_compiled_shader = std::move(compiled_shader); }

    const ShaderProgramRef &GetShaderProgram() const
        { return m_shader_program; }

    void Init();

private:
    CompiledShader m_compiled_shader;
    ShaderProgramRef m_shader_program;
};

class ShaderManagerSystem
{
public:
    Handle<Shader> GetOrCreate(
        const ShaderDefinition &definition
    );

    Handle<Shader> GetOrCreate(
        Name name,
        const ShaderProperties &props = { }
    );

private:
    HashMap<ShaderDefinition, WeakHandle<Shader>>   m_map;
    std::mutex                                      m_mutex;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

