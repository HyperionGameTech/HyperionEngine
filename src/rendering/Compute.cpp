#include "Compute.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {
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

        engine->render_scheduler.Enqueue([this, engine](...) {
            return m_pipeline.Create(
                engine->GetDevice(),
                m_shader->GetShaderProgram(),
                &engine->GetInstance()->GetDescriptorPool()
            );
        });

        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            SetReady(false);

            engine->render_scheduler.Enqueue([this, engine](...) {
               return m_pipeline.Destroy(engine->GetDevice()); 
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

} // namespace hyperion::v2
