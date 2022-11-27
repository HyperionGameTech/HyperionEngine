#include <rendering/UIRenderer.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateUIDescriptors) : RenderCommand
{
    SizeType component_index;
    FixedArray<renderer::ImageView *, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateUIDescriptors)(
        SizeType component_index,
        FixedArray<renderer::ImageView *, max_frames_in_flight> &&image_views
    ) : component_index(component_index),
        image_views(std::move(image_views))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::UI_TEXTURE)
                ->SetElementSRV(UInt(component_index), image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyUIDescriptors) : RenderCommand
{
    SizeType component_index;

    RENDER_COMMAND(DestroyUIDescriptors)(
        SizeType component_index
    ) : component_index(component_index)
    {
    }

    virtual Result operator()()
    {
        auto result = renderer::Result::OK;

        // remove descriptors from global descriptor set
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            // set to placeholder data.
            descriptor_set
                ->GetDescriptor(DescriptorKey::UI_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = UInt(component_index),
                    .image_view = &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        return result;
    }
};

UIRenderer::UIRenderer(Handle<Scene> &&scene)
    : EngineComponentBase(),
      RenderComponent(),
      m_scene(std::move(scene))
{
}

UIRenderer::~UIRenderer()
{
    if (!IsInitCalled()) {
        return;
    }

    SetReady(false);

    RenderCommands::Push<RENDER_COMMAND(DestroyUIDescriptors)>(GetComponentIndex());

    HYP_SYNC_RENDER();
}

void UIRenderer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    InitObject(m_scene);

    CreateFramebuffers();
    CreateDescriptors();

    SetReady(true);
}

void UIRenderer::CreateFramebuffers()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index] = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_UI].GetFramebuffers()[frame_index];
    }
}

void UIRenderer::CreateDescriptors()
{
    // create descriptors in render thread
    RenderCommands::Push<RENDER_COMMAND(CreateUIDescriptors)>(
        GetComponentIndex(),
        FixedArray<renderer::ImageView *, max_frames_in_flight> {
            m_framebuffers[0]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView(),
            m_framebuffers[1]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView()
        }
    );
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved() { }

void UIRenderer::OnUpdate(GameCounter::TickUnit delta) { }

void UIRenderer::OnRender(Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    m_framebuffers[frame_index]->BeginCapture(command_buffer);
    Engine::Get()->render_state.BindScene(m_scene.Get());

    for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_UI).GetRenderGroups()) {
        renderer_instance->Render(frame);
    }

    Engine::Get()->render_state.UnbindScene();
    m_framebuffers[frame_index]->EndCapture(command_buffer);
}

} // namespace hyperion::v2
