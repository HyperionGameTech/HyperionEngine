#include <rendering/EntityDrawCollection.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/Renderer.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <Engine.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(UpdateRenderGroupDrawables) : RenderCommand
{
    RenderGroup *render_group;
    Array<EntityDrawProxy> drawables;

    RENDER_COMMAND(UpdateRenderGroupDrawables)(RenderGroup *render_group, Array<EntityDrawProxy> &&drawables)
        : render_group(render_group),
          drawables(std::move(drawables))
    {
    }

    virtual Result operator()()
    {
        render_group->SetDrawProxies(std::move(drawables));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateDrawCollectionRenderSide) : RenderCommand
{
    RC<EntityDrawCollection> collection;
    RenderableAttributeSet attributes;
    EntityDrawCollection::EntityList entity_list;

    RENDER_COMMAND(UpdateDrawCollectionRenderSide)(
        const RC<EntityDrawCollection> &collection,
        const RenderableAttributeSet &attributes,
        EntityDrawCollection::EntityList &&entity_list
    ) : collection(collection),
        attributes(attributes),
        entity_list(std::move(entity_list))
    {
    }

    virtual Result operator()()
    {
        collection->SetEntityList(attributes, std::move(entity_list));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

EntityDrawCollection::ThreadType EntityDrawCollection::GetThreadType()
{
    const UInt thread_id = Threads::CurrentThreadID().value;

    return thread_id == THREAD_GAME ? THREAD_TYPE_GAME
        : (thread_id == THREAD_RENDER ? THREAD_TYPE_RENDER : THREAD_TYPE_INVALID);
}

FlatMap<RenderableAttributeSet, EntityDrawCollection::EntityList> &EntityDrawCollection::GetEntityList()
{
    const ThreadType thread_type = GetThreadType();

    AssertThrowMsg(thread_type != THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_entities[UInt(thread_type)];
}

FlatMap<RenderableAttributeSet, EntityDrawCollection::EntityList> &EntityDrawCollection::GetEntityList(ThreadType thread_type)
{
    AssertThrowMsg(thread_type != THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_entities[UInt(thread_type)];
}

void EntityDrawCollection::Insert(const RenderableAttributeSet &attributes, const EntityDrawProxy &entity)
{
    GetEntityList(THREAD_TYPE_GAME)[attributes].drawables.PushBack(entity);
}

void EntityDrawCollection::SetEntityList(const RenderableAttributeSet &attributes, EntityList &&entity_list)
{
    GetEntityList().Set(attributes, std::move(entity_list));
}

void EntityDrawCollection::ClearEntities()
{
    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &it : GetEntityList()) {
        it.second.drawables.Clear();
    }
}


RenderList::RenderList()
    : m_draw_collection(new EntityDrawCollection)
{
}

void RenderList::ClearEntities()
{
    AssertThrow(m_draw_collection != nullptr);

    m_draw_collection->ClearEntities();
}

void RenderList::UpdateRenderGroups()
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    for (auto &it : m_draw_collection->GetEntityList(EntityDrawCollection::THREAD_TYPE_GAME)) {
        const RenderableAttributeSet &attributes = it.first;
        EntityDrawCollection::EntityList &entity_list = it.second;

        if (!entity_list.render_group.IsValid()) {
            auto render_group_it = m_render_groups.Find(attributes);

            if (render_group_it == m_render_groups.End() || !render_group_it->second) {
                DebugLog(
                    LogType::Debug,
                    "Creating RenderGroup for attributes hash %llu\n",
                    attributes.GetHashCode()
                );

                Handle<RenderGroup> render_group = Engine::Get()->CreateRenderGroup(attributes);

                if (!render_group.IsValid()) {
                    DebugLog(LogType::Error, "Render group not valid for attribute set %llu!\n", attributes.GetHashCode().Value());

                    continue;
                }

                InitObject(render_group);

                auto insert_result = m_render_groups.Set(attributes, std::move(render_group));
                AssertThrow(insert_result.second);

                render_group_it = insert_result.first;
            }

            entity_list.render_group = render_group_it->second;
        }

        PUSH_RENDER_COMMAND(
            UpdateDrawCollectionRenderSide,
            m_draw_collection,
            attributes,
            std::move(entity_list)
        );
    }
}

void RenderList::PushEntityToRender(
    const Handle<Camera> &camera,
    const Handle<Entity> &entity,
    const RenderableAttributeSet *override_attributes
)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    AssertThrow(entity.IsValid());
    AssertThrow(entity->IsRenderable());

    // TEMP
    if (!entity->GetDrawProxy().mesh_id) {
        return;
    }

    AssertThrow(entity->GetDrawProxy().mesh_id.IsValid());

    const Handle<Framebuffer> &framebuffer = camera
        ? camera->GetFramebuffer()
        : Handle<Framebuffer>::empty;

    RenderableAttributeSet attributes = entity->GetRenderableAttributes();

    if (framebuffer) {
        attributes.framebuffer_id = framebuffer->GetID();
    }

    if (override_attributes) {
        attributes.shader_def = override_attributes->shader_def ? override_attributes->shader_def : attributes.shader_def;
        
        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        // TODO: Do not calculate at render time; pre-calculate
        if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes != attributes.shader_def.properties.CalculateVertexAttributes()) {
            attributes.shader_def.properties.SetVertexAttributes(entity->GetRenderableAttributes().mesh_attributes.vertex_attributes);
        }

        attributes.material_attributes = override_attributes->material_attributes;
        attributes.stencil_state = override_attributes->stencil_state;
    }

    m_draw_collection->Insert(attributes, entity->GetDrawProxy());
}

void RenderList::Render(
    Frame *frame,
    const Scene *scene,
    const Handle<Camera> &camera,
    const void *push_constant_ptr,
    SizeType push_constant_size
)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertThrow(m_draw_collection != nullptr);

    AssertThrowMsg(camera.IsValid(), "Cannot render with invalid Camera");
    AssertThrowMsg(camera->GetFramebuffer().IsValid(), "Camera has no Framebuffer is attached");

    CommandBuffer *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    camera->GetFramebuffer()->BeginCapture(frame_index, command_buffer);

    Engine::Get()->GetRenderState().BindScene(scene);
    Engine::Get()->GetRenderState().BindCamera(camera.Get());

    for (auto &it : m_draw_collection->GetEntityList(EntityDrawCollection::THREAD_TYPE_RENDER)) {
        const RenderableAttributeSet &attributes = it.first;
        auto &entity_list = it.second;

        AssertThrow(entity_list.render_group.IsValid());

        entity_list.render_group->SetDrawProxies(entity_list.drawables);

        AssertThrowMsg(
            attributes.framebuffer_id == camera->GetFramebuffer()->GetID(),
            "Given Camera's Framebuffer ID does not match RenderList item framebuffer ID -- invalid data passed?"
        );

        if (push_constant_ptr && push_constant_size) {
            entity_list.render_group->GetPipeline()->SetPushConstants(push_constant_ptr, push_constant_size);
        }

        entity_list.render_group->Render(frame);
    }

    Engine::Get()->GetRenderState().UnbindCamera();
    Engine::Get()->GetRenderState().UnbindScene();

    camera->GetFramebuffer()->EndCapture(frame_index, command_buffer);
}

void RenderList::Reset()
{
    AssertThrow(m_draw_collection != nullptr);
    // Threads::AssertOnThread(THREAD_GAME);

    // perform full clear
    *m_draw_collection = { };
}

} // namespace hyperion::v2