#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Renderer.hpp>
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

#pragma endregion

void EntityDrawCollection::Insert(const RenderableAttributeSet &attributes, const EntityDrawProxy &entity)
{
    m_entities[attributes].PushBack(entity);
}

void EntityDrawCollection::Reset()
{
    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot.
    for (auto &it : m_entities) {
        it.second.Clear();
    }
}


void RenderList::UpdateRenderGroups()
{
    Threads::AssertOnThread(THREAD_GAME);

    for (auto &it : m_draw_collection) {
        const RenderableAttributeSet &attributes = it.first;
        Array<EntityDrawProxy> &draw_proxies = it.second;

        auto render_group_it = m_render_groups.Find(attributes);

        if (render_group_it == m_render_groups.End() || !render_group_it->second) {
            Handle<RenderGroup> render_group = Engine::Get()->CreateRenderGroup(attributes);

            if (!render_group.IsValid()) {
                DebugLog(LogType::Error, "Render group not valid for attribute set %llu!\n", attributes.GetHashCode().Value());

                continue;
            }

            auto insert_result = m_render_groups.Insert(attributes, std::move(render_group));
            AssertThrow(insert_result.second);

            render_group_it = insert_result.first;
        }
        
        PUSH_RENDER_COMMAND(
            UpdateRenderGroupDrawables,
            render_group_it->second.Get(),
            std::move(draw_proxies)
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

    AssertThrow(entity.IsValid());

    const Handle<Framebuffer> &framebuffer = camera
        ? camera->GetFramebuffer()
        : Handle<Framebuffer>::empty;

    RenderableAttributeSet attributes = entity->GetRenderableAttributes();

    if (framebuffer) {
        attributes.framebuffer_id = framebuffer->GetID();
    }

    if (override_attributes) {
        attributes.shader_id = override_attributes->shader_id ? override_attributes->shader_id : attributes.shader_id;
        attributes.material_attributes = override_attributes->material_attributes;
        attributes.stencil_state = override_attributes->stencil_state;
    }

    m_draw_collection.Insert(attributes, entity->GetDrawProxy());
}

void RenderList::Reset()
{
    // Threads::AssertOnThread(THREAD_GAME);

    // perform full clear
    m_draw_collection = { };
    m_render_groups.Clear();
}

} // namespace hyperion::v2