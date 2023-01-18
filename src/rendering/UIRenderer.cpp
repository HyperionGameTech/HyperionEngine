#include <rendering/UIRenderer.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateUIDescriptors) : RenderCommand
{
    SizeType component_index;
    renderer::ImageView *image_view;

    RENDER_COMMAND(CreateUIDescriptors)(
        SizeType component_index,
        renderer::ImageView *image_view
    ) : component_index(component_index),
        image_view(image_view)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::UI_TEXTURE)
                ->SetElementSRV(UInt(component_index), image_view);
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
                ->SetElementSRV(component_index, &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());
        }

        return result;
    }
};

UIRenderer::UIRenderer(const Handle<Scene> &scene)
    : EngineComponentBase(),
      RenderComponent(),
      m_scene(scene)
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

    CreateFramebuffer();
    CreateDescriptors();

    AssertThrow(m_scene.IsValid());
    AssertThrow(m_scene->GetCamera().IsValid());

    m_scene->GetCamera()->SetFramebuffer(m_framebuffer);

    InitObject(m_scene);

    SetReady(true);
}

void UIRenderer::CreateFramebuffer()
{
    m_framebuffer = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_UI].GetFramebuffer();
}

void UIRenderer::CreateDescriptors()
{
    // create descriptors in render thread
    RenderCommands::Push<RENDER_COMMAND(CreateUIDescriptors)>(
        GetComponentIndex(),
        m_framebuffer->GetAttachmentUsages()[0]->GetImageView()
    );
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved() { }

void UIRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    m_scene->CollectEntities(
        m_render_list,
        m_scene->GetCamera(),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_UI,
                .cull_faces = FaceCullMode::NONE
            }
        )
    );

    m_render_list.UpdateRenderGroups();
}

void UIRenderer::OnRender(Frame *frame)
{
    m_render_list.Render(frame, m_scene.Get(), m_scene->GetCamera());
}

} // namespace hyperion::v2
