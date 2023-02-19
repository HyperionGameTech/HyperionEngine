#include "Compute.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(CreateComputeShader) : RenderCommand
{
    ComputePipelineRef pipeline;
    ShaderProgramRef shader_program;

    RENDER_COMMAND(CreateComputeShader)(
        const ComputePipelineRef &pipeline,
        const ShaderProgramRef &shader_program
    ) : pipeline(pipeline),
        shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Create(
            g_engine->GetGPUDevice(),
            shader_program,
            &g_engine->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

#pragma endregion
    
ComputePipeline::ComputePipeline(Handle<Shader> &&shader)
    : EngineComponentBase(),
      m_shader(std::move(shader)),
      m_pipeline(RenderObjects::Make<renderer::ComputePipeline>())
{
}

ComputePipeline::ComputePipeline(
    Handle<Shader> &&shader,
    const Array<const DescriptorSet *> &used_descriptor_sets
) : EngineComponentBase(),
    m_shader(std::move(shader)),
    m_pipeline(RenderObjects::Make<renderer::ComputePipeline>(used_descriptor_sets))
{
}

ComputePipeline::~ComputePipeline()
{
    SetReady(false);

    SafeRelease(std::move(m_pipeline));
    m_shader.Reset();
}

void ComputePipeline::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();
    
    if (InitObject(m_shader)) {
        PUSH_RENDER_COMMAND(
            CreateComputeShader,
            m_pipeline,
            m_shader->GetShaderProgram()
        );

        SetReady(true);
    }
}

} // namespace hyperion::v2
