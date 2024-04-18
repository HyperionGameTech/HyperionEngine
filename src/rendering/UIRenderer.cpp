/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

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

/*! \brief Custom CollectEntities function to use instead of Scene::CollectEntities.
    Loop over nodes instead of Entities from EntitySets, as sort order is based on parent node for UI objects.
    \note Definitely much less efficient when compared to Scene::CollectEntities, as we are not iterating over a contiguous array
    of references to components. Plus, there are additional checks in place, as we are losing some of the conveniences that method brings.     
*/
void UIRenderer::CollectEntities(const Node *node)
{
    if (!node) {
        return;
    }

    const ID<Entity> entity = node->GetEntity();

    if (entity.IsValid()) {
        AssertThrow(node->GetScene() != nullptr);
        AssertThrow(node->GetScene()->GetEntityManager() != nullptr);

        MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
        TransformComponent *transform_component = node->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(entity);
        BoundingBoxComponent *bounding_box_component = node->GetScene()->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(entity);

        if (mesh_component != nullptr && transform_component != nullptr && bounding_box_component != nullptr) {
            if (mesh_component->mesh.IsValid() && mesh_component->material.IsValid()) {
                m_render_list.PushEntityToRender(
                    m_scene->GetCamera(),
                    entity,
                    mesh_component->mesh,
                    mesh_component->material,
                    mesh_component->skeleton,
                    transform_component->transform.GetMatrix(),
                    mesh_component->previous_model_matrix,
                    bounding_box_component->world_aabb,
                    nullptr
                );
            }
        }
    }

    for (auto &it : node->GetChildren()) {
        CollectEntities(it.Get());
    }
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

    m_render_list.ClearEntities();

    CollectEntities(m_scene->GetRoot().Get());

    // m_scene->CollectEntities(
    //     m_render_list,
    //     m_scene->GetCamera()
    // );

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
