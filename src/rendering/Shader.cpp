#include "Shader.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Result;

void ShaderGlobals::Create()
{
    auto *device = Engine::Get()->GetDevice();

    scenes.Create(device);
    materials.Create(device);
    objects.Create(device);
    skeletons.Create(device);
    lights.Create(device);
    shadow_maps.Create(device);
    env_probes.Create(device);
    immediate_draws.Create(device);
    cubemap_uniforms.Create(device, sizeof(CubemapUniforms));

    textures.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = Engine::Get()->GetDevice();

    cubemap_uniforms.Destroy(device);
    env_probes.Destroy(device);

    scenes.Destroy(device);
    objects.Destroy(device);
    materials.Destroy(device);
    skeletons.Destroy(device);
    lights.Destroy(device);
    shadow_maps.Destroy(device);
    immediate_draws.Destroy(device);
}

struct RENDER_COMMAND(CreateShaderProgram) : RenderCommandBase2
{
    renderer::ShaderProgram *shader_program;
    // Array<SubShader> subshaders;
    std::vector<SubShader> subshaders;

    RENDER_COMMAND(CreateShaderProgram)(
        renderer::ShaderProgram *shader_program,
        // Array<SubShader> &&subshaders
        const std::vector<SubShader> &subshaders
    ) : shader_program(shader_program),
        subshaders(subshaders)
    {
    }

    virtual ~RENDER_COMMAND(CreateShaderProgram)()
    {
        DebugLog(LogType::Error, "Destruct %s  %p\n", __FUNCTION__, (void*)this);
    }

    virtual Result operator()()
    {
        for (const SubShader &sub_shader : subshaders) {
            HYPERION_BUBBLE_ERRORS(shader_program->AttachShader(
                Engine::Get()->GetInstance()->GetDevice(),
                sub_shader.type,
                sub_shader.spirv
            ));
        }

        return shader_program->Create(Engine::Get()->GetDevice());
    }
};

struct RENDER_COMMAND(DestroyShaderProgram) : RenderCommandBase2
{
    renderer::ShaderProgram *shader_program;

    RENDER_COMMAND(DestroyShaderProgram)(renderer::ShaderProgram *shader_program)
        : shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return shader_program->Destroy(Engine::Get()->GetDevice());
    }
};

Shader::Shader(const std::vector<SubShader> &sub_shaders)
    : EngineComponentBase(),
      m_shader_program(std::make_unique<ShaderProgram>()),
      m_sub_shaders(sub_shaders)
{
}

Shader::Shader(const CompiledShader &compiled_shader)
    : EngineComponentBase(),
      m_shader_program(std::make_unique<ShaderProgram>())
{
    if (compiled_shader.IsValid()) {
        for (SizeType index = 0; index < compiled_shader.modules.Size(); index++) {
            const auto &byte_buffer = compiled_shader.modules[index];

            if (byte_buffer.Empty()) {
                continue;
            }

            m_sub_shaders.push_back(SubShader {
                .type = ShaderModule::Type(index),
                .spirv = {
                    .bytes = byte_buffer
                }
            });
        }
    }
}

Shader::~Shader()
{
    Teardown();
}

void Shader::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    for (const auto &sub_shader : m_sub_shaders) {
        AssertThrowMsg(sub_shader.spirv.bytes.Any(), "Shader data missing");
    }

    RenderCommands::Push<RENDER_COMMAND(CreateShaderProgram)>(
        m_shader_program.get(),
        m_sub_shaders
        // Array<SubShader>(m_sub_shaders.data(), m_sub_shaders.size())
        //Array<SubShader>(m_sub_shaders.data(), m_sub_shaders.data() + m_sub_shaders.size())
    );

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        RenderCommands::Push<RENDER_COMMAND(DestroyShaderProgram)>(m_shader_program.get());
        
        HYP_FLUSH_RENDER_QUEUE();
    });
}

} // namespace hyperion
