/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADER_HPP
#define HYPERION_SHADER_HPP

#include <core/Base.hpp>
#include <core/Name.hpp>
#include <core/threading/Mutex.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <HashCode.hpp>

namespace hyperion {

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

class HYP_API Shader
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

    void SetCompiledShader(const CompiledShader &compiled_shader);
    void SetCompiledShader(CompiledShader &&compiled_shader);

    const ShaderProgramRef &GetShaderProgram() const
        { return m_shader_program; }

    void Init();

private:
    CompiledShader      m_compiled_shader;
    ShaderProgramRef    m_shader_program;
};

class HYP_API ShaderManagerSystem
{
public:
    static ShaderManagerSystem *GetInstance();

    Handle<Shader> GetOrCreate(
        const ShaderDefinition &definition
    );

    Handle<Shader> GetOrCreate(
        Name name,
        const ShaderProperties &props = { }
    );

private:
    HashMap<ShaderDefinition, WeakHandle<Shader>>   m_map;
    Mutex                                           m_mutex;
};

} // namespace hyperion

#endif // !HYPERION_SHADER_HPP

