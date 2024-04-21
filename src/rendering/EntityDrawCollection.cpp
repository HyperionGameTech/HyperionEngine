/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/containers/Array.hpp>
#include <core/threading/Threads.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/Util.hpp>

#include <rendering/EntityDrawCollection.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/RenderGroup.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/animation/Skeleton.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr bool do_parallel_collection = true;

#pragma region Render commands

struct RENDER_COMMAND(UpdateDrawCollectionRenderSide) : renderer::RenderCommand
{
    RC<EntityDrawCollection>            collection;
    RenderableAttributeSet              attributes;
    EntityDrawCollection::EntityList    entity_list;

    RENDER_COMMAND(UpdateDrawCollectionRenderSide)(
        const RC<EntityDrawCollection> &collection,
        const RenderableAttributeSet &attributes,
        EntityDrawCollection::EntityList &&entity_list
    ) : collection(collection),
        attributes(attributes),
        entity_list(std::move(entity_list))
    {
    }

    virtual ~RENDER_COMMAND(UpdateDrawCollectionRenderSide)() override = default;

    virtual Result operator()() override
    {
        collection->SetRenderSideList(attributes, std::move(entity_list));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList()
{
    const ThreadType thread_type = Threads::GetThreadType();

    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

const FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList() const
{
    const ThreadType thread_type = Threads::GetThreadType();

    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList(ThreadType thread_type)
{
    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

const FixedArray<ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList(ThreadType thread_type) const
{
    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

void EntityDrawCollection::InsertEntityWithAttributes(const RenderableAttributeSet &attributes, EntityDrawData entity_draw_data)
{
    GetEntityList(ThreadType::THREAD_TYPE_GAME)[BucketToPassType(attributes.GetMaterialAttributes().bucket)][attributes].entity_draw_datas.PushBack(std::move(entity_draw_data));
}

void EntityDrawCollection::SetRenderSideList(const RenderableAttributeSet &attributes, EntityList &&entity_list)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

    auto render_side_resources_it = m_render_side_resources[pass_type].Find(attributes);

    if (render_side_resources_it == m_render_side_resources[pass_type].End()) {
        const auto insert_result = m_render_side_resources[pass_type].Insert(attributes, { });
        
        render_side_resources_it = insert_result.first;
    }

    RenderResourceManager &render_side_resources = render_side_resources_it->second;

    Bitset prev_bitsets[3] = {
        render_side_resources.GetResourceUsageMap<Mesh>()->usage_bits,
        render_side_resources.GetResourceUsageMap<Material>()->usage_bits,
        render_side_resources.GetResourceUsageMap<Skeleton>()->usage_bits
    };

    Bitset new_bitsets[3] = { }; // mesh, material, skeleton

    // prevent these objects from going out of scope while rendering is happening
    for (const EntityDrawData &entity_draw_data : entity_list.entity_draw_datas) {
        new_bitsets[0].Set(entity_draw_data.mesh_id.ToIndex(), true);
        new_bitsets[1].Set(entity_draw_data.material_id.ToIndex(), true);
        new_bitsets[2].Set(entity_draw_data.skeleton_id.ToIndex(), true);
    }

    // Set each bitset to be the bits that are in the previous bitset, but not in the new one.
    // This will give us a list of IDs that were removed.
    for (uint i = 0; i < ArraySize(prev_bitsets); i++) {
        // If any of the bitsets are different sizes, resize them to match the largest one,
        // this makes ~ and & operations work as expected
        if (prev_bitsets[i].NumBits() > new_bitsets[i].NumBits()) {
            new_bitsets[i].Resize(prev_bitsets[i].NumBits());
        } else if (prev_bitsets[i].NumBits() < new_bitsets[i].NumBits()) {
            prev_bitsets[i].Resize(new_bitsets[i].NumBits());
        }

        SizeType first_set_bit_index;

        Bitset removed_id_bits = prev_bitsets[i] & ~new_bitsets[i];

        // Iterate over the bits that were removed, and drop the references to them.
        while ((first_set_bit_index = removed_id_bits.FirstSetBitIndex()) != -1) {
            // Remove the reference
            switch (i) {
            case 0:
                render_side_resources.SetIsUsed(ID<Mesh>::FromIndex(first_set_bit_index), false);
                break;
            case 1:
                render_side_resources.SetIsUsed(ID<Material>::FromIndex(first_set_bit_index), false);
                break;
            case 2:
                render_side_resources.SetIsUsed(ID<Skeleton>::FromIndex(first_set_bit_index), false);
                break;
            }

            removed_id_bits.Set(first_set_bit_index, false);
        }

        Bitset newly_added_id_bits = new_bitsets[i] & ~prev_bitsets[i];

        while ((first_set_bit_index = newly_added_id_bits.FirstSetBitIndex()) != -1) {
            // Create a reference to it in the resources list.
            switch (i) {
            case 0:
                render_side_resources.SetIsUsed(ID<Mesh>::FromIndex(first_set_bit_index), true);
                break;
            case 1:
                render_side_resources.SetIsUsed(ID<Material>::FromIndex(first_set_bit_index), true);
                break;
            case 2:
                render_side_resources.SetIsUsed(ID<Skeleton>::FromIndex(first_set_bit_index), true);
                break;
            }

            newly_added_id_bits.Set(first_set_bit_index, false);
        }
    }

    auto &mappings = GetEntityList()[pass_type];
    auto it = mappings.Find(attributes);

    if (it != mappings.End()) {
            it->second = std::move(entity_list);
    } else {
        if (!entity_list.entity_draw_datas.Empty()) {
            mappings.Set(attributes, std::move(entity_list));
        }
    }
}

void EntityDrawCollection::ClearEntities()
{
    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &collection_per_pass_type : GetEntityList()) {
        for (auto &it : collection_per_pass_type) {
            it.second.entity_draw_datas.Clear();
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
    : m_draw_collection(new EntityDrawCollection())
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
    Threads::AssertOnThread(ThreadName::THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    using IteratorType = ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>::Iterator;

    Array<IteratorType> iterators;
    Array<Pair<RenderableAttributeSet, Handle<RenderGroup>>> added_render_groups;

    for (auto &collection_per_pass_type : m_draw_collection->GetEntityList(ThreadType::THREAD_TYPE_GAME)) {
        for (auto &it : collection_per_pass_type) {
            iterators.PushBack(&it);
        }
    }

    added_render_groups.Resize(iterators.Size());

    const auto UpdateRenderGroupForAttributeSet = [this, &added_render_groups](IteratorType it, uint index, uint)
    {
        const RenderableAttributeSet &attributes = it->first;
        EntityDrawCollection::EntityList &entity_list = it->second;

        if (!entity_list.render_group.IsValid()) {
            if (added_render_groups[index].second.IsValid()) {
#ifdef HYP_DEBUG_MODE
                AssertThrowMsg(attributes == added_render_groups[index].first, "Attributes do not match with assigned index of %u", index);
#endif

                entity_list.render_group = added_render_groups[index].second;
            } else {
                auto render_group_it = m_render_groups.Find(attributes);

                if (render_group_it != m_render_groups.End()) {
                    entity_list.render_group = render_group_it->second.Lock();

                    // DebugLog(LogType::Debug, "Acquire lock for render group %llu\t%u\n", attributes.GetHashCode().Value(), attributes.GetMaterialAttributes().bucket);
                }

                if (!entity_list.render_group.IsValid()) {
                    Handle<RenderGroup> render_group = g_engine->CreateRenderGroup(attributes);

                    DebugLog(LogType::Debug, "Create render group %llu (#%u)\n", attributes.GetHashCode().Value(), render_group.GetID().Value());

#ifdef HYP_DEBUG_MODE
                    if (!render_group.IsValid()) {
                        DebugLog(LogType::Error, "Render group not valid for attribute set %llu!\n", attributes.GetHashCode().Value());

                        return;
                    }
#endif

                    InitObject(render_group);

                    added_render_groups[index] = { attributes, render_group };

                    entity_list.render_group = std::move(render_group);
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
        TaskSystem::GetInstance().ParallelForEach(
            TaskThreadPoolName::THREAD_POOL_RENDER_COLLECT,
            iterators,
            UpdateRenderGroupForAttributeSet
        );
    } else {
        for (uint index = 0; index < iterators.Size(); index++) {
            UpdateRenderGroupForAttributeSet(iterators[index], index, 0 /* unused */);
        }
    }

    for (auto &it : added_render_groups) {
        const RenderableAttributeSet &attributes = it.first;
        Handle<RenderGroup> &render_group = it.second;

        if (render_group.IsValid()) {
            m_render_groups.Set(attributes, render_group);
        }
    }
}

void RenderList::PushEntityToRender(
    const Handle<Camera> &camera,
    ID<Entity> entity_id,
    const Handle<Mesh> &mesh,
    const Handle<Material> &material,
    const Handle<Skeleton> &skeleton,
    const Matrix4 &model_matrix,
    const Matrix4 &previous_model_matrix,
    const BoundingBox &aabb,
    const RenderableAttributeSet *override_attributes
)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(mesh.IsValid());
    AssertThrow(entity_id.IsValid());

    const Handle<Framebuffer> &framebuffer = camera
        ? camera->GetFramebuffer()
        : Handle<Framebuffer>::empty;

    RenderableAttributeSet attributes {
        mesh.IsValid() ? mesh->GetMeshAttributes() : MeshAttributes { },
        material.IsValid() ? material->GetRenderAttributes() : MaterialAttributes { }
    };

    if (framebuffer) {
        attributes.SetFramebufferID(framebuffer->GetID());
    }

    if (override_attributes) {
        if (const ShaderDefinition &override_shader_definition = override_attributes->GetShaderDefinition()) {
            attributes.SetShaderDefinition(override_shader_definition);
        }
        
        ShaderDefinition shader_definition = override_attributes->GetShaderDefinition().IsValid()
            ? override_attributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        AssertThrow(shader_definition.IsValid());
#endif

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

        if (mesh_vertex_attributes != shader_definition.GetProperties().GetRequiredVertexAttributes()) {
            shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
        }

        MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
        new_material_attributes.shader_definition = shader_definition;
        // do not override bucket!
        new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(new_material_attributes);
        attributes.SetStencilState(override_attributes->GetStencilState());
    }

    m_draw_collection->InsertEntityWithAttributes(attributes, EntityDrawData {
        entity_id,
        mesh.GetID(),
        material.GetID(),
        skeleton.GetID(),
        model_matrix,
        previous_model_matrix,
        aabb,
        attributes.GetMaterialAttributes().bucket
    });
}

void RenderList::CollectDrawCalls(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data
)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>::Iterator;

    Array<IteratorType> iterators;

    for (auto &collection_per_pass_type : m_draw_collection->GetEntityList(ThreadType::THREAD_TYPE_RENDER)) {
        for (auto &it : collection_per_pass_type) {
            const RenderableAttributeSet &attributes = it.first;

            const Bucket bucket = bucket_bits.Test(uint(attributes.GetMaterialAttributes().bucket)) ? attributes.GetMaterialAttributes().bucket : BUCKET_INVALID;

            if (bucket == BUCKET_INVALID) {
                continue;
            }

            iterators.PushBack(&it);
        }
    }

    if constexpr (do_parallel_collection) {
        TaskSystem::GetInstance().ParallelForEach(
            TaskThreadPoolName::THREAD_POOL_RENDER,
            iterators,
            [](IteratorType it, uint, uint)
            {
                EntityDrawCollection::EntityList &entity_list = it->second;
                Handle<RenderGroup> &render_group = entity_list.render_group;

                AssertThrow(render_group.IsValid());

                render_group->SetEntityDrawDatas(entity_list.entity_draw_datas);
                render_group->CollectDrawCalls();
            }
        );
    } else {
        for (IteratorType it : iterators) {
            EntityDrawCollection::EntityList &entity_list = it->second;
            Handle<RenderGroup> &render_group = entity_list.render_group;

            AssertThrow(render_group.IsValid());

            render_group->SetEntityDrawDatas(entity_list.entity_draw_datas);
            render_group->CollectDrawCalls();
        }
    }

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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    AssertThrowMsg(camera.IsValid(), "Cannot render with invalid Camera");

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    if (framebuffer) {
        framebuffer->BeginCapture(frame_index, command_buffer);
    }

    g_engine->GetRenderState().BindCamera(camera.Get());

    for (const auto &collection_per_pass_type : m_draw_collection->GetEntityList(ThreadType::THREAD_TYPE_RENDER)) {
        for (const auto &it : collection_per_pass_type) {
            const RenderableAttributeSet &attributes = it.first;
            const EntityDrawCollection::EntityList &entity_list = it.second;

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            if (!bucket_bits.Test(uint(bucket))) {
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

void RenderList::ExecuteDrawCallsInLayers(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(m_camera.IsValid());
    AssertThrowMsg(m_camera->GetFramebuffer().IsValid(), "Camera has no Framebuffer attached");

    ExecuteDrawCallsInLayers(frame, m_camera, m_camera->GetFramebuffer(), bucket_bits, cull_data, push_constant);
}

void RenderList::ExecuteDrawCallsInLayers(
    Frame *frame,
    const Handle<Framebuffer> &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(m_camera.IsValid());

    ExecuteDrawCallsInLayers(frame, m_camera, framebuffer, bucket_bits, cull_data, push_constant);
}

void RenderList::ExecuteDrawCallsInLayers(
    Frame *frame,
    const Handle<Camera> &camera,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(camera.IsValid());
    AssertThrowMsg(camera->GetFramebuffer().IsValid(), "Camera has no Framebuffer is attached");

    ExecuteDrawCallsInLayers(frame, camera, camera->GetFramebuffer(), bucket_bits, cull_data, push_constant);
}

void RenderList::ExecuteDrawCallsInLayers(
    Frame *frame,
    const Handle<Camera> &camera,
    const Handle<Framebuffer> &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);
    AssertThrowMsg(camera.IsValid(), "Cannot render with invalid Camera");

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    if (framebuffer) {
        framebuffer->BeginCapture(frame_index, command_buffer);
    }

    g_engine->GetRenderState().BindCamera(camera.Get());

    using IteratorType = ArrayMap<RenderableAttributeSet, EntityDrawCollection::EntityList>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto &collection_per_pass_type : m_draw_collection->GetEntityList(ThreadType::THREAD_TYPE_RENDER)) {
        for (const auto &it : collection_per_pass_type) {
            iterators.PushBack(&it);
        }
    }

    std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
    {
        // sort by z layer
        return lhs->first.GetMaterialAttributes().layer < rhs->first.GetMaterialAttributes().layer;
    });

    for (SizeType index = 0; index < iterators.Size(); index++) {
        const auto &it = *iterators[index];

        const RenderableAttributeSet &attributes = it.first;
        const EntityDrawCollection::EntityList &entity_list = it.second;

        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

        if (!bucket_bits.Test(uint(bucket))) {
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

    g_engine->GetRenderState().UnbindCamera();

    if (framebuffer) {
        framebuffer->EndCapture(frame_index, command_buffer);
    }
}

void RenderList::Reset()
{
    AssertThrow(m_draw_collection != nullptr);
    // Threads::AssertOnThread(ThreadName::THREAD_GAME);

    // perform full clear
    *m_draw_collection = { };
}

} // namespace hyperion