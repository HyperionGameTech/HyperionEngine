#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <ui/UIObject.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(SetUITextureInGlobalDescriptorSet) : renderer::RenderCommand
{
    uint            component_index;
    ImageViewRef    image_view;

    RENDER_COMMAND(SetUITextureInGlobalDescriptorSet)(
        uint component_index,
        ImageViewRef image_view
    ) : component_index(component_index),
        image_view(std::move(image_view))
    {
        AssertThrow(this->image_view != nullptr);
    }

    virtual ~RENDER_COMMAND(SetUITextureInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(UITexture), image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetUITextureFromGlobalDescriptorSet) : renderer::RenderCommand
{
    uint component_index;

    RENDER_COMMAND(UnsetUITextureFromGlobalDescriptorSet)(
        uint component_index
    ) : component_index(component_index)
    {
    }

    virtual ~RENDER_COMMAND(UnsetUITextureFromGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        // remove descriptors from global descriptor set
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(UITexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

UIRenderer::UIRenderer(Name name, Handle<Scene> scene)
    : RenderComponent(name),
      m_scene(std::move(scene))
{
}

UIRenderer::~UIRenderer()
{
    PUSH_RENDER_COMMAND(UnsetUITextureFromGlobalDescriptorSet, GetComponentIndex());

    HYP_SYNC_RENDER();
}

void UIRenderer::Init()
{
    CreateFramebuffer();
    CreateDescriptors();

    AssertThrow(m_scene.IsValid());
    AssertThrow(m_scene->GetCamera().IsValid());

    m_scene->GetCamera()->SetFramebuffer(m_framebuffer);
    InitObject(m_scene);

    m_render_list.SetCamera(m_scene->GetCamera());
}

void UIRenderer::CreateFramebuffer()
{
    m_framebuffer = g_engine->GetDeferredSystem()[Bucket::BUCKET_UI].GetFramebuffer();
}

void UIRenderer::CreateDescriptors()
{
    // create descriptors in render thread
    PUSH_RENDER_COMMAND(
        SetUITextureInGlobalDescriptorSet, 
        GetComponentIndex(),
        m_framebuffer->GetAttachmentUsages()[0]->GetImageView()
    );
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved() { }

void UIRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    m_scene->Update(delta);

    m_scene->CollectEntities(
        m_render_list,
        m_scene->GetCamera()
    );

    m_render_list.UpdateRenderGroups();
}

void UIRenderer::OnRender(Frame *frame)
{
    g_engine->GetRenderState().BindScene(m_scene.Get());

    m_render_list.CollectDrawCalls(
        frame,
        Bitset((1 << BUCKET_UI)),
        nullptr,    /* cull_data */
        true        /* sort_z_layer */
    );

    m_render_list.ExecuteDrawCalls(
        frame,
        Bitset((1 << BUCKET_UI)),
        nullptr     /* cull_data */
    );

    g_engine->GetRenderState().UnbindScene();
}

} // namespace hyperion::v2
