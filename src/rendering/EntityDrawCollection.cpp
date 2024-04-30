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

struct RENDER_COMMAND(SetRenderSideList) : renderer::RenderCommand
{
    RC<EntityDrawCollection>    collection;
    RenderableAttributeSet      attributes;
    EntityList                  entity_list;

    RENDER_COMMAND(SetRenderSideList)(
        const RC<EntityDrawCollection> &collection,
        const RenderableAttributeSet &attributes,
        EntityList &&entity_list
    ) : collection(collection),
        attributes(attributes),
        entity_list(std::move(entity_list))
    {
    }

    virtual ~RENDER_COMMAND(SetRenderSideList)() override = default;

    virtual Result operator()() override
    {
        collection->SetRenderSideList(attributes, std::move(entity_list));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateRenderSideResources) : renderer::RenderCommand
{
    RC<EntityDrawCollection>    collection;
    RenderResourceManager       resources;

    RENDER_COMMAND(UpdateRenderSideResources)(
        const RC<EntityDrawCollection> &collection,
        RenderResourceManager &&resources
    ) : collection(collection),
        resources(std::move(resources))
    {
    }

    virtual ~RENDER_COMMAND(UpdateRenderSideResources)() override = default;

    virtual Result operator()() override
    {
        collection->UpdateRenderSideResources(resources);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region EntityList

EntityList::EntityList(const EntityList &other)
    : entity_draw_datas(other.entity_draw_datas),
      render_group(other.render_group),
      usage_bits(other.usage_bits)
{
}

EntityList &EntityList::operator=(const EntityList &other)
{
    if (this == &other) {
        return *this;
    }

    entity_draw_datas = other.entity_draw_datas;
    render_group = other.render_group;
    usage_bits = other.usage_bits;

    return *this;
}

EntityList::EntityList(EntityList &&other) noexcept
    : entity_draw_datas(std::move(other.entity_draw_datas)),
      render_group(std::move(other.render_group)),
      usage_bits(std::move(other.usage_bits))
{
}

EntityList &EntityList::operator=(EntityList &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    entity_draw_datas = std::move(other.entity_draw_datas);
    render_group = std::move(other.render_group);
    usage_bits = std::move(other.usage_bits);

    return *this;
}

#pragma endregion EntityList

#pragma region EntityDrawCollection

FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList()
{
    const ThreadType thread_type = Threads::GetThreadType();

    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

const FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList() const
{
    const ThreadType thread_type = Threads::GetThreadType();

    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList(ThreadType thread_type)
{
    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

const FixedArray<ArrayMap<RenderableAttributeSet, EntityList>, PASS_TYPE_MAX> &EntityDrawCollection::GetEntityList(ThreadType thread_type) const
{
    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_lists[uint(thread_type)];
}

void EntityDrawCollection::InsertEntityWithAttributes(const RenderableAttributeSet &attributes, const EntityDrawData &entity_draw_data)
{
    auto &list = GetEntityList(ThreadType::THREAD_TYPE_GAME)[BucketToPassType(attributes.GetMaterialAttributes().bucket)][attributes];

    if (entity_draw_data.mesh_id.IsValid()) {
        list.usage_bits[RESOURCE_USAGE_TYPE_MESH].Set(entity_draw_data.mesh_id.ToIndex(), true);
    }

    if (entity_draw_data.material_id.IsValid()) {
        list.usage_bits[RESOURCE_USAGE_TYPE_MATERIAL].Set(entity_draw_data.material_id.ToIndex(), true);
    }

    if (entity_draw_data.skeleton_id.IsValid()) {
        list.usage_bits[RESOURCE_USAGE_TYPE_SKELETON].Set(entity_draw_data.skeleton_id.ToIndex(), true);
    }
    
    list.entity_draw_datas.PushBack(entity_draw_data);
}

void EntityDrawCollection::SetRenderSideList(const RenderableAttributeSet &attributes, EntityList &&entity_list)
{
    DebugLog(LogType::Debug, "SetRenderSideList()  %p\n", this);

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

    auto &mappings = GetEntityList()[pass_type];
    
    mappings.Set(attributes, std::move(entity_list));
}

void EntityDrawCollection::UpdateRenderSideResources(RenderResourceManager &new_resources)
{
    DebugLog(LogType::Debug, "UpdateRenderSideResources()  %p\n", this);

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    RenderResourceManager &render_side_resources = m_current_render_side_resources;
    render_side_resources.TakeUsagesFrom(new_resources);
}

void EntityDrawCollection::ClearEntities()
{
    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &collection_per_pass_type : GetEntityList()) {
        for (auto &it : collection_per_pass_type) {
            it.second.ClearEntities();
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

#pragma endregion EntityDrawCollection

#pragma region RenderList

RenderList::RenderList()
    : m_draw_collection(new EntityDrawCollection())
{
}

RenderList::RenderList(const Handle<Camera> &camera)
    : m_camera(camera)
{
}

RenderList::~RenderList()
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

    using IteratorType = ArrayMap<RenderableAttributeSet, EntityList>::Iterator;

    Array<IteratorType> iterators;
    Array<Pair<RenderableAttributeSet, Handle<RenderGroup>>> added_render_groups;

    for (auto &collection_per_pass_type : m_draw_collection->GetEntityList(ThreadType::THREAD_TYPE_GAME)) {
        for (auto &it : collection_per_pass_type) {
            iterators.PushBack(&it);
        }
    }

    added_render_groups.Resize(iterators.Size());

    Array<FixedArray<Bitset, ResourceUsageType::RESOURCE_USAGE_TYPE_MAX>> bitsets;
    bitsets.Resize(iterators.Size());

    const auto CollectDrawCallsForAttributeSet = [this, &added_render_groups, &bitsets](IteratorType it, uint index, uint)
    {
        const RenderableAttributeSet &attributes = it->first;
        EntityList &entity_list = it->second;

        if (!entity_list.render_group.IsValid()) {
            if (added_render_groups[index].second.IsValid()) {
#ifdef HYP_DEBUG_MODE
                AssertThrowMsg(attributes == added_render_groups[index].first, "Attributes do not match with assigned index of %u", index);
#endif

                entity_list.render_group = added_render_groups[index].second;
            } else {
                auto render_group_it = m_render_groups.Find(attributes);

                if (render_group_it != m_render_groups.End()) {
                    entity_list.render_group = render_group_it->second;

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

                    // Don't collect DrawCalls this frame -- RenderGroup needs an extra frame to finish initializing
                    return;
                }
            }
        }

        for (uint resource_type_index = 0; resource_type_index < RESOURCE_USAGE_TYPE_MAX; resource_type_index++) {
            bitsets[index][resource_type_index] |= entity_list.usage_bits[resource_type_index];
        }

        entity_list.render_group->CollectDrawCalls(entity_list.entity_draw_datas);
    };
    

    for (uint resource_type_index = 0; resource_type_index < RESOURCE_USAGE_TYPE_MAX; resource_type_index++) {
        Bitset combined;

        for (uint i = 0; i < bitsets.Size(); i++) {
            combined |= bitsets[i][resource_type_index];
        }

        m_resources.CollectNeededResourcesForBits(ResourceUsageType(resource_type_index), combined);
    }

    TaskSystem::GetInstance().ParallelForEach(
        TaskThreadPoolName::THREAD_POOL_RENDER,
        iterators,
        CollectDrawCallsForAttributeSet
    );

    struct RENDER_COMMAND(UpdateRenderListRenderGroups) : public renderer::RenderCommand
    {
        RenderList                                                  &render_list;
        Array<Pair<RenderableAttributeSet, Handle<RenderGroup>>>    added_render_groups;

        RENDER_COMMAND(UpdateRenderListRenderGroups)(
            RenderList &render_list,
            Array<Pair<RenderableAttributeSet, Handle<RenderGroup>>> &&added_render_groups
        ) : render_list(render_list),
            added_render_groups(std::move(added_render_groups))
        {
        }

        virtual ~RENDER_COMMAND(UpdateRenderListRenderGroups)() override = default;

        virtual Result operator()() override
        {
            for (uint i = 0; i < added_render_groups.Size(); i++) {
                const auto &pair = added_render_groups[i];

                if (pair.second.IsValid()) {
                    render_list.m_render_groups.Set(pair.first, pair.second);
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(UpdateRenderListRenderGroups, *this, std::move(added_render_groups));
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

    //  @NOTE !! This method should be renamed if UpdateRenderGroups() now collects draw calls..
    // it only performs occlusion culling.

    if (use_draw_indirect && cull_data != nullptr) {
        for (auto &it : m_render_groups) {
            if (!it.second.IsValid()) {
                continue;
            }

            it.second->PerformOcclusionCulling(frame, cull_data);
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

    for (auto &it : m_render_groups) {
        const RenderableAttributeSet &attributes = it.first;
        const Handle<RenderGroup> &render_group = it.second;

        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

        if (!bucket_bits.Test(uint(bucket))) {
            continue;
        }

        AssertThrow(render_group.IsValid());

        if (framebuffer) {
            AssertThrowMsg(
                attributes.GetFramebufferID() == framebuffer->GetID(),
                "Given Framebuffer's ID does not match RenderList item's framebuffer ID -- invalid data passed?"
            );
        }

        if (push_constant) {
            render_group->GetPipeline()->SetPushConstants(push_constant.ptr, push_constant.size);
        }

        if (use_draw_indirect && cull_data != nullptr) {
            render_group->PerformRenderingIndirect(frame);
        } else {
            render_group->PerformRendering(frame);
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

    using IteratorType = HashMap<RenderableAttributeSet, Handle<RenderGroup>>::ConstIterator;

    Array<IteratorType> iterators;
    iterators.Reserve(m_render_groups.Size());

    for (auto it = m_render_groups.Begin(); it != m_render_groups.End(); ++it) {
        iterators.PushBack(it);
    }

    std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
    {
        // sort by z layer
        return lhs->first.GetMaterialAttributes().layer < rhs->first.GetMaterialAttributes().layer;
    });

    for (SizeType index = 0; index < iterators.Size(); index++) {
        const auto &it = *iterators[index];

        const RenderableAttributeSet &attributes = it.first;
        const Handle<RenderGroup> &render_group = it.second;

        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

        if (!bucket_bits.Test(uint(bucket))) {
            continue;
        }

        AssertThrow(render_group.IsValid());

        if (framebuffer) {
            AssertThrowMsg(
                attributes.GetFramebufferID() == framebuffer->GetID(),
                "Given Framebuffer's ID does not match RenderList item's framebuffer ID -- invalid data passed?"
            );
        }

        if (push_constant) {
            render_group->GetPipeline()->SetPushConstants(push_constant.ptr, push_constant.size);
        }

        if (use_draw_indirect && cull_data != nullptr) {
            render_group->PerformRenderingIndirect(frame);
        } else {
            render_group->PerformRendering(frame);
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

#pragma endregion RenderList

} // namespace hyperion