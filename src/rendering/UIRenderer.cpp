/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <ui/UIStage.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;

UIRenderer::UIRenderer(Name name, RC<UIStage> ui_stage)
    : RenderComponent(name),
      m_ui_stage(std::move(ui_stage))
{
}

UIRenderer::~UIRenderer()
{
    g_engine->GetFinalPass().SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::Init()
{
    CreateFramebuffer();

    AssertThrow(m_ui_stage != nullptr);

    AssertThrow(m_ui_stage->GetScene() != nullptr);
    AssertThrow(m_ui_stage->GetScene()->GetCamera().IsValid());

    m_ui_stage->GetScene()->GetCamera()->SetFramebuffer(m_framebuffer);

    m_render_list.SetCamera(m_ui_stage->GetScene()->GetCamera());

    g_engine->GetFinalPass().SetUITexture(CreateObject<Texture>(
        m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage(),
        m_framebuffer->GetAttachmentUsages()[0]->GetImageView()
    ));
}

void UIRenderer::CreateFramebuffer()
{
    m_framebuffer = g_engine->GetDeferredSystem()[Bucket::BUCKET_UI].GetFramebuffer();
}

/*! \brief Custom CollectObjects function to use instead of Scene::CollectEntities.
    Loop over nodes instead of Entities from EntitySets, as sort order is based on parent node for UI objects.
    \note Definitely much less efficient when compared to Scene::CollectEntities, as we are not iterating over a contiguous array
    of references to components. Plus, there are additional checks in place, as we are losing some of the conveniences that method brings.     
*/
void UIRenderer::CollectObjects(const NodeProxy &node, Array<RC<UIObject>> &out_objects)
{
    if (!node.IsValid()) {
        return;
    }

    const ID<Entity> entity = node->GetEntity();

    if (entity.IsValid()) {
        MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
        TransformComponent *transform_component = node->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(entity);
        BoundingBoxComponent *bounding_box_component = node->GetScene()->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(entity);

        if (mesh_component != nullptr && transform_component != nullptr && bounding_box_component != nullptr) {
            UIComponent *ui_component = node->GetScene()->GetEntityManager()->TryGetComponent<UIComponent>(entity);

            if (ui_component && ui_component->ui_object) {
                // Visibility affects all child nodes as well, so return from here.
                if (!ui_component->ui_object->IsVisible()) {
                    return;
                }

                AssertThrow(ui_component->ui_object->GetNode().IsValid());

                out_objects.PushBack(ui_component->ui_object);
            }
        }
    }

    Array<Pair<NodeProxy, RC<UIObject>>> children;
    children.Reserve(node->GetChildren().Size());

    for (const auto &it : node->GetChildren()) {
        if (!it.IsValid()) {
            continue;
        }

        UIComponent *ui_component = it->GetEntity().IsValid()
            ? it->GetScene()->GetEntityManager()->TryGetComponent<UIComponent>(it->GetEntity())
            : nullptr;

        children.PushBack({
            it,
            ui_component != nullptr
                ? ui_component->ui_object
                : nullptr
        });
    }

    for (const Pair<NodeProxy, RC<UIObject>> &it : children) {
        CollectObjects(it.first, out_objects);
    }
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved()
{
    g_engine->GetFinalPass().SetUITexture(Handle<Texture>::empty);
}

HYP_DISABLE_OPTIMIZATION;
void UIRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    m_render_list.ClearEntities();

    Array<RC<UIObject>> objects;
    CollectObjects(m_ui_stage->GetScene()->GetRoot(), objects);

    std::sort(objects.Begin(), objects.End(), [](const RC<UIObject> &lhs, const RC<UIObject> &rhs)
    {
        AssertThrow(lhs != nullptr);
        AssertThrow(lhs->GetNode() != nullptr);

        AssertThrow(rhs != nullptr);
        AssertThrow(rhs->GetNode() != nullptr);

        return lhs->GetNode()->GetWorldTranslation().z < rhs->GetNode()->GetWorldTranslation().z;
    });

    for (uint i = 0; i < objects.Size(); i++) {
        const RC<UIObject> &object = objects[i];
        AssertThrow(object != nullptr);

        object->SetDrawableLayer(DrawableLayer(0, i));

        const ID<Entity> entity = object->GetNode()->GetEntity();
        const NodeProxy &node = object->GetNode();

        MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
        TransformComponent *transform_component = node->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(entity);
        BoundingBoxComponent *bounding_box_component = node->GetScene()->GetEntityManager()->TryGetComponent<BoundingBoxComponent>(entity);

        AssertThrow(mesh_component != nullptr);
        AssertThrow(transform_component != nullptr);
        AssertThrow(bounding_box_component != nullptr);

        m_render_list.PushEntityToRender(
            node->GetScene()->GetCamera(),
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

    // m_ui_stage->GetScene()->CollectEntities(
    //     m_render_list,
    //     m_ui_stage->GetScene()->GetCamera()
    // );

    m_render_list.UpdateRenderGroups();
}
HYP_ENABLE_OPTIMIZATION;

void UIRenderer::OnRender(Frame *frame)
{
    g_engine->GetRenderState().BindScene(m_ui_stage->GetScene().Get());

    m_render_list.CollectDrawCalls(
        frame,
        Bitset((1 << BUCKET_UI)),
        /* cull_data */ nullptr    
    );

    m_render_list.ExecuteDrawCallsInLayers(
        frame,
        Bitset((1 << BUCKET_UI)),
        /* cull_data */ nullptr
    );

    g_engine->GetRenderState().UnbindScene();
}

} // namespace hyperion
