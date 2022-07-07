#include "RenderPass.hpp"

#include "../Engine.hpp"

namespace hyperion::v2 {

RenderPass::RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode)
    : EngineComponentBase(),
      m_render_pass(stage, mode)
{
}

RenderPass::RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode, UInt num_multiview_layers)
    : EngineComponentBase(),
      m_render_pass(stage, mode, num_multiview_layers)
{
}

RenderPass::~RenderPass()
{
    Teardown();
}

void RenderPass::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_RENDER_PASSES, [this](Engine *engine) {
        engine->render_scheduler.Enqueue([this, engine](...) {
            return m_render_pass.Create(engine->GetDevice());
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_RENDER_PASSES, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine](...) {
                return m_render_pass.Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }), engine);
    }));
}

} // namespace hyperion::v2
