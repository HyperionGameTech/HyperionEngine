#include <core/lib/DynArray.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/RenderGroup.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <Engine.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

static constexpr bool do_parallel_collection = true;

#pragma region Render commands

struct RENDER_COMMAND(UpdateDrawCollectionRenderSide) : RenderCommand
{
    RC<EntityDrawCollection>            collection;
    RenderableAttributeSet              attributes;
    EntityDrawCollection::EntityList     entity_list;

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
        collection->SetRenderSideList(attributes, std::move(entity_list));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList()
{
    const ThreadType thread_type = Threads::GetThreadType();

    AssertThrowMsg(thread_type != THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[UInt(thread_type)];
}

const FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList() const
{
    const ThreadType thread_type = Threads::GetThreadType();

    AssertThrowMsg(thread_type != THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[UInt(thread_type)];
}

FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList(ThreadType thread_type)
{
    AssertThrowMsg(thread_type != THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[UInt(thread_type)];
}

const FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList(ThreadType thread_type) const
{
    AssertThrowMsg(thread_type != THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[UInt(thread_type)];
}

void EntityDrawCollection::Insert(const RenderableAttributeSet &attributes, const EntityDrawProxy &entity)
{
    GetEntityList(THREAD_TYPE_GAME)[BucketToPassType(attributes.GetMaterialAttributes().bucket)][attributes].drawables.PushBack(entity);
}

void EntityDrawCollection::SetRenderSideList(const RenderableAttributeSet &attributes, EntityList &&entity_list)
{
    auto &mapping = GetEntityList()[BucketToPassType(attributes.GetMaterialAttributes().bucket)];

    // TODO...
    // Rather than recreate new render side resources,
    // could we reuse the same object, using bitwise xor on the bitset
    // to track IDs? Then jus iterate through bits in that and cross reference
    // them with bits in the new one, and drop any references that are in that?

    auto it = mapping.Find(attributes);

    if (it != mapping.End()) {
        RenderResourceManager previous_resources = std::move(it->second.render_side_resources);

        // prevent these objects from going out of scope while rendering is happening
        for (const EntityDrawProxy &draw_proxy : entity_list.drawables) {
            entity_list.render_side_resources.SetIsUsed(
                draw_proxy.mesh_id,
                previous_resources.TakeResourceUsage(draw_proxy.mesh_id),
                true
            );

            entity_list.render_side_resources.SetIsUsed(
                draw_proxy.material_id,
                previous_resources.TakeResourceUsage(draw_proxy.material_id),
                true
            );

            entity_list.render_side_resources.SetIsUsed(
                draw_proxy.skeleton_id,
                previous_resources.TakeResourceUsage(draw_proxy.skeleton_id),
                true
            );
        }

        it->second = std::move(entity_list);
    } else {
        // prevent these objects from going out of scope while rendering is happening
        for (const EntityDrawProxy &draw_proxy : entity_list.drawables) {
            entity_list.render_side_resources.SetIsUsed(
                draw_proxy.mesh_id,
                true
            );

            entity_list.render_side_resources.SetIsUsed(
                draw_proxy.material_id,
                true
            );

            entity_list.render_side_resources.SetIsUsed(
                draw_proxy.skeleton_id,
                true
            );
        }

        mapping.Set(attributes, std::move(entity_list));
    }
}

void EntityDrawCollection::ClearEntities()
{
    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &collection_per_pass_type : GetEntityList()) {
        for (auto &it : collection_per_pass_type) {
            it.second.drawables.Clear();
        }
    }
}

HashCode EntityDrawCollection::CalculateCombinedAttributesHashCode() const
{
    HashCode hc;

    for (auto &collection_per_pass_type : GetEntityList()) {
        for (auto &it : collection_per_pass_type) {
            hc.Add(it.first.GetHashCode());
        }
    }

    return hc;
}


RenderList::RenderList()
{
}

RenderList::RenderList(const Handle<Camera> &camera)
    : m_camera(camera)
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

    using IteratorType = ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>::Iterator;

    Array<IteratorType> iterators;
    Array<Pair<RenderableAttributeSet, Handle<RenderGroup>>> added_render_groups;

    for (auto &collection_per_pass_type : m_draw_collection->GetEntityList(THREAD_TYPE_GAME)) {
        for (auto &it : collection_per_pass_type) {
            iterators.PushBack(&it);
        }
    }

    added_render_groups.Resize(iterators.Size());

    const auto UpdateRenderGroupForAttributeSet = [this, &added_render_groups](IteratorType it, UInt index, UInt) {
        const RenderableAttributeSet &attributes = it->first;
        EntityDrawCollection::EntityList &entity_list = it->second;

        if (!entity_list.render_group.IsValid()) {
            if (added_render_groups[index].second.IsValid()) {
#ifdef HYP_DEBUG_MODE
                AssertThrowMsg(attributes == added_render_groups[index].first, "Attributes to not match with assigned index of %u", index);
#endif

                entity_list.render_group = added_render_groups[index].second;
            } else {
                auto render_group_it = m_render_groups.Find(attributes);

                if (render_group_it == m_render_groups.End() || !render_group_it->second) {
                    Handle<RenderGroup> render_group = g_engine->CreateRenderGroup(attributes);

                    if (!render_group.IsValid()) {
                        DebugLog(LogType::Error, "Render group not valid for attribute set %llu!\n", attributes.GetHashCode().Value());

                        return;
                    }

                    InitObject(render_group);

                    entity_list.render_group = render_group;

                    added_render_groups[index] = { attributes, std::move(render_group) };
                } else {
                    entity_list.render_group = render_group_it->second;
                }
            }
        }

        PUSH_RENDER_COMMAND(
            UpdateDrawCollectionRenderSide,
            m_draw_collection,
            attributes,
            std::move(entity_list)
        );
    };

    if constexpr (do_parallel_collection) {
        TaskSystem::GetInstance().ParallelForEach(THREAD_POOL_RENDER_COLLECT, iterators, UpdateRenderGroupForAttributeSet);
    } else {
        for (UInt index = 0; index < iterators.Size(); index++) {
            UpdateRenderGroupForAttributeSet(iterators[index], index, 0);
        }
    }

    for (auto &it : added_render_groups) {
        const RenderableAttributeSet &attributes = it.first;
        Handle<RenderGroup> &render_group = it.second;

        if (render_group.IsValid()) {
            m_render_groups.Set(attributes, std::move(render_group));
        }
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
        attributes.SetFramebufferID(framebuffer->GetID());
    }

    if (override_attributes) {
        if (const ShaderDefinition &override_shader_definition = override_attributes->GetShaderDefinition()) {
            attributes.SetShaderDefinition(override_shader_definition);
        }
        
        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

        if (mesh_vertex_attributes != attributes.GetShaderDefinition().GetProperties().GetRequiredVertexAttributes()) {
            ShaderDefinition new_shader_definition = attributes.GetShaderDefinition();
            new_shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);

            attributes.SetShaderDefinition(new_shader_definition);
        }

        // do not override bucket!
        MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
        new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(new_material_attributes);
        attributes.SetStencilState(override_attributes->GetStencilState());
    }

    m_draw_collection->Insert(attributes, entity->GetDrawProxy());
}

void RenderList::CollectDrawCalls(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data
)
{
    Threads::AssertOnThread(THREAD_RENDER);

    using IteratorType = ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>::Iterator;

    Array<IteratorType> iterators;

    for (auto &collection_per_pass_type : m_draw_collection->GetEntityList(THREAD_TYPE_RENDER)) {
        for (auto &it : collection_per_pass_type) {
            const RenderableAttributeSet &attributes = it.first;

            const Bucket bucket = bucket_bits.Test(UInt(attributes.GetMaterialAttributes().bucket)) ? attributes.GetMaterialAttributes().bucket : BUCKET_INVALID;

            if (bucket == BUCKET_INVALID) {
                continue;
            }

            iterators.PushBack(&it);
        }
    }

    if constexpr (do_parallel_collection) {
        TaskSystem::GetInstance().ParallelForEach(THREAD_POOL_RENDER, iterators, [](IteratorType it, UInt, UInt) {
            EntityDrawCollection::EntityList &entity_list = it->second;
            Handle<RenderGroup> &render_group = entity_list.render_group;

            AssertThrow(render_group.IsValid());

            render_group->SetDrawProxies(entity_list.drawables);
            render_group->CollectDrawCalls();
        });
    } else {
        for (IteratorType it : iterators) {
            EntityDrawCollection::EntityList &entity_list = it->second;
            Handle<RenderGroup> &render_group = entity_list.render_group;

            AssertThrow(render_group.IsValid());

            render_group->SetDrawProxies(entity_list.drawables);
            render_group->CollectDrawCalls();
        }
    }

    // for (SizeType index = 0; index < iterators.Size(); index++) {
    //     (*iterators[index]).second.render_group->CollectDrawCalls();
    // }

    if (use_draw_indirect && cull_data != nullptr) {
        for (SizeType index = 0; index < iterators.Size(); index++) {
            (*iterators[index]).second.render_group->PerformOcclusionCulling(frame, cull_data);
        }
    }
}

void RenderList::ExecuteDrawCalls(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(m_camera.IsValid());
    AssertThrowMsg(m_camera->GetFramebuffer().IsValid(), "Camera has no Framebuffer attached");

    ExecuteDrawCalls(frame, m_camera, m_camera->GetFramebuffer(), bucket_bits, cull_data, push_constant);
}

void RenderList::ExecuteDrawCalls(
    Frame *frame,
    const Handle<Framebuffer> &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(m_camera.IsValid());

    ExecuteDrawCalls(frame, m_camera, framebuffer, bucket_bits, cull_data, push_constant);
}

void RenderList::ExecuteDrawCalls(
    Frame *frame,
    const Handle<Camera> &camera,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(camera.IsValid());
    AssertThrowMsg(camera->GetFramebuffer().IsValid(), "Camera has no Framebuffer is attached");

    ExecuteDrawCalls(frame, camera, camera->GetFramebuffer(), bucket_bits, cull_data, push_constant);
}

void RenderList::ExecuteDrawCalls(
    Frame *frame,
    const Handle<Camera> &camera,
    const Handle<Framebuffer> &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    AssertThrowMsg(camera.IsValid(), "Cannot render with invalid Camera");

    CommandBuffer *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    if (framebuffer) {
        framebuffer->BeginCapture(frame_index, command_buffer);
    }

    g_engine->GetRenderState().BindCamera(camera.Get());

    for (const auto &collection_per_pass_type : m_draw_collection->GetEntityList(THREAD_TYPE_RENDER)) {
        for (const auto &it : collection_per_pass_type) {
            const RenderableAttributeSet &attributes = it.first;
            const EntityDrawCollection::EntityList &entity_list = it.second;

            const Bucket bucket = bucket_bits.Test(UInt(attributes.GetMaterialAttributes().bucket)) ? attributes.GetMaterialAttributes().bucket : BUCKET_INVALID;

            if (bucket == BUCKET_INVALID) {
                continue;
            }

            AssertThrow(entity_list.render_group.IsValid());

            if (framebuffer) {
                AssertThrowMsg(
                    attributes.GetFramebufferID() == framebuffer->GetID(),
                    "Given Framebuffer's ID does not match RenderList item's framebuffer ID -- invalid data passed?"
                );
            }

            if (push_constant) {
                entity_list.render_group->GetPipeline()->SetPushConstants(push_constant.ptr, push_constant.size);
            }

            if (use_draw_indirect && cull_data != nullptr) {
                entity_list.render_group->PerformRenderingIndirect(frame);
            } else {
                entity_list.render_group->PerformRendering(frame);
            }
        }
    }

    g_engine->GetRenderState().UnbindCamera();

    if (framebuffer) {
        framebuffer->EndCapture(frame_index, command_buffer);
    }
}

void RenderList::Reset()
{
    AssertThrow(m_draw_collection != nullptr);
    // Threads::AssertOnThread(THREAD_GAME);

    // perform full clear
    *m_draw_collection = { };
}

} // namespace hyperion::v2