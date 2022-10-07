#include <rendering/UIRenderer.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>

namespace hyperion::v2 {

UIRenderer::UIRenderer(Handle<Scene> &&scene)
    : EngineComponentBase(),
      RenderComponent(),
      m_scene(std::move(scene))
{
}

UIRenderer::~UIRenderer()
{
    Teardown();
}

void UIRenderer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    engine->InitObject(m_scene);

    CreateFramebuffers(engine);
    CreateDescriptors(engine);

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        auto *engine = GetEngine();

        engine->GetRenderScheduler().Enqueue([this, engine](...) {
            auto result = renderer::Result::OK;

            // remove descriptors from global descriptor set
            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

                // set to placeholder data.
                descriptor_set
                    ->GetDescriptor(DescriptorKey::UI_TEXTURE)
                    ->SetSubDescriptor({
                        .element_index = GetComponentIndex(),
                        .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
                    });
            }

            return result;
        });

        HYP_FLUSH_RENDER_QUEUE(engine);
    });
}

void UIRenderer::CreateFramebuffers(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index] = engine->GetDeferredSystem()[Bucket::BUCKET_UI].GetFramebuffers()[frame_index];
    }
}

void UIRenderer::CreateDescriptors(Engine *engine)
{
    // create descriptors in render thread
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::UI_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = GetComponentIndex(),
                    .image_view = m_framebuffers[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView()
                });
        }

        HYPERION_RETURN_OK;
    });
}

// called from game thread
void UIRenderer::InitGame(Engine *engine) { }

void UIRenderer::OnRemoved() { }

void UIRenderer::OnUpdate(Engine *engine, GameCounter::TickUnit delta) { }

void UIRenderer::OnRender(Engine *engine, Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    m_framebuffers[frame_index]->BeginCapture(command_buffer);
    engine->render_state.BindScene(m_scene.Get());

    for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_UI).GetRendererInstances()) {
        renderer_instance->Render(engine, frame);
    }

    engine->render_state.UnbindScene();
    m_framebuffers[frame_index]->EndCapture(command_buffer);
}

} // namespace hyperion::v2
