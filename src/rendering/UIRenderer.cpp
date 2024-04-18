/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <ui/UIObject.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;

UIRenderer::UIRenderer(Name name, Handle<Scene> scene)
    : RenderComponent(name),
      m_scene(std::move(scene))
{
}

UIRenderer::~UIRenderer()
{
    g_engine->GetFinalPass().SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::Init()
{
    CreateFramebuffer();

    AssertThrow(m_scene.IsValid());
    AssertThrow(m_scene->GetCamera().IsValid());

    m_scene->GetCamera()->SetFramebuffer(m_framebuffer);
    InitObject(m_scene);

    m_render_list.SetCamera(m_scene->GetCamera());

    g_engine->GetFinalPass().SetUITexture(CreateObject<Texture>(
        m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage(),
        m_framebuffer->GetAttachmentUsages()[0]->GetImageView()
    ));
}

void UIRenderer::CreateFramebuffer()
{
    m_framebuffer = g_engine->GetDeferredSystem()[Bucket::BUCKET_UI].GetFramebuffer();
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved()
{
    g_engine->GetFinalPass().SetUITexture(Handle<Texture>::empty);
}

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
        nullptr     /* cull_data */
    );

    m_render_list.ExecuteDrawCallsInLayers(
        frame,
        Bitset((1 << BUCKET_UI)),
        nullptr      /* cull_data */
    );

    g_engine->GetRenderState().UnbindScene();
}

} // namespace hyperion
