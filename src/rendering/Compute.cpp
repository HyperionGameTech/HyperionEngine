#include "Compute.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

struct RENDER_COMMAND(CreateComputeShader) : RenderCommandBase2
{
    ComputePipeline &pipeline;

    RENDER_COMMAND(CreateComputeShader)(ComputePipeline &pipeline)
        : pipeline(pipeline)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        return pipeline.GetPipeline()->Create(
            pipeline.GetEngine()->GetDevice(),
            pipeline.GetShader()->GetShaderProgram(),
            &engine->GetInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(DestroyComputeShader) : RenderCommandBase2
{
    ComputePipeline &pipeline;

    RENDER_COMMAND(DestroyComputeShader)(ComputePipeline &pipeline)
        : pipeline(pipeline)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        return pipeline.GetPipeline()->Destroy(engine->GetDevice());
    }
};
    
ComputePipeline::ComputePipeline(Handle<Shader> &&shader)
    : EngineComponentBase(),
      m_shader(std::move(shader))
{
}

ComputePipeline::ComputePipeline(
    Handle<Shader> &&shader,
    const Array<const DescriptorSet *> &used_descriptor_sets
) : EngineComponentBase(),
    m_pipeline(used_descriptor_sets),
    m_shader(std::move(shader))
{
}

ComputePipeline::~ComputePipeline()
{
    Teardown();
}

void ComputePipeline::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    AssertThrow(engine->InitObject(m_shader));

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_COMPUTE_PIPELINES, [this](...) {
        auto *engine = GetEngine();

        RenderCommands::Push<RENDER_COMMAND(CreateComputeShader)>(*this);

        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            SetReady(false);

            RenderCommands::Push<RENDER_COMMAND(DestroyComputeShader)>(*this);
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

} // namespace hyperion::v2
