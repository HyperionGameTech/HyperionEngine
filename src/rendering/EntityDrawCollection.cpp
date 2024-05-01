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

struct RENDER_COMMAND(RebuildProxyGroups) : renderer::RenderCommand
{
    RC<EntityDrawCollection>                collection;
    Array<RenderProxy>                      added_proxies;
    Array<ID<Entity>>                       removed_proxies;

    Handle<Framebuffer>                     framebuffer;
    Optional<RenderableAttributeSet>        override_attributes;

    RENDER_COMMAND(RebuildProxyGroups)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        const Handle<Framebuffer> &framebuffer = Handle<Framebuffer>::empty,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups)() override = default;

    virtual Result operator()() override
    {
        // Clear out all proxy groups and recreate them
        collection->ClearProxyGroups();

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (RenderProxy &proxy : added_proxies) {
            const ID<Entity> entity = proxy.entity;

            Handle<Mesh> &mesh = proxy.mesh;
            Handle<Material> &material = proxy.material;

            RenderableAttributeSet attributes {
                mesh.IsValid() ? mesh->GetMeshAttributes() : MeshAttributes { },
                material.IsValid() ? material->GetRenderAttributes() : MaterialAttributes { }
            };

            if (framebuffer.IsValid()) {
                attributes.SetFramebufferID(framebuffer->GetID());
            }

            if (override_attributes.HasValue()) {
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

            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);
            
            // Add proxy to group
            RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];

            if (!render_proxy_group.render_group.IsValid()) {
                // Create RenderGroup
                Handle<RenderGroup> render_group = g_engine->CreateRenderGroup(attributes);

                DebugLog(LogType::Debug, "Create render group %llu (#%u)\n", attributes.GetHashCode().Value(), render_group.GetID().Value());

#ifdef HYP_DEBUG_MODE
                if (!render_group.IsValid()) {
                    DebugLog(LogType::Error, "Render group not valid for attribute set %llu!\n", attributes.GetHashCode().Value());

                    continue;
                }
#endif

                InitObject(render_group);

                render_proxy_group.render_group = std::move(render_group);
            }

            render_proxy_group.render_proxies.PushBack(proxy);

            proxy_list.Add(entity, std::move(proxy));
        }

        for (const ID<Entity> &entity : removed_proxies) {
            proxy_list.MarkToRemove(entity);
        }

        proxy_list.Advance(RPLAA_PERSIST);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region EntityList

RenderProxyGroup::RenderProxyGroup(const RenderProxyGroup &other)
    : render_proxies(other.render_proxies),
      render_group(other.render_group)
{
}

RenderProxyGroup &RenderProxyGroup::operator=(const RenderProxyGroup &other)
{
    if (this == &other) {
        return *this;
    }

    render_proxies = other.render_proxies;
    render_group = other.render_group;

    return *this;
}

RenderProxyGroup::RenderProxyGroup(RenderProxyGroup &&other) noexcept
    : render_proxies(std::move(other.render_proxies)),
      render_group(std::move(other.render_group))
{
}

RenderProxyGroup &RenderProxyGroup::operator=(RenderProxyGroup &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    render_proxies = std::move(other.render_proxies);
    render_group = std::move(other.render_group);

    return *this;
}

#pragma endregion EntityList

#pragma region EntityDrawCollection

FixedArray<ArrayMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &EntityDrawCollection::GetProxyGroups()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    return m_proxy_groups;
}

const FixedArray<ArrayMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &EntityDrawCollection::GetProxyGroups() const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    return m_proxy_groups;
}

RenderProxyList &EntityDrawCollection::GetProxyList(ThreadType thread_type)
{
    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_proxy_lists[uint(thread_type)];
}

const RenderProxyList &EntityDrawCollection::GetProxyList(ThreadType thread_type) const
{
    AssertThrowMsg(thread_type != ThreadType::THREAD_TYPE_INVALID, "Invalid thread for calling method");

    return m_proxy_lists[uint(thread_type)];
}

void EntityDrawCollection::AddRenderProxy(ThreadType thread_type, const RenderableAttributeSet &attributes, const RenderProxy &proxy)
{
    auto &group = GetProxyGroups()[BucketToPassType(attributes.GetMaterialAttributes().bucket)][attributes];

    group.render_proxies.PushBack(proxy);
}

void EntityDrawCollection::AddRenderProxy(const RenderableAttributeSet &attributes, const RenderProxy &proxy)
{
    AddRenderProxy(Threads::GetThreadType(), attributes, proxy);
}

void EntityDrawCollection::ClearProxyGroups()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &proxy_group : GetProxyGroups()) {
        for (auto &it : proxy_group) {
            it.second.ClearProxies();
        }
    }
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

void RenderList::UpdateOnRenderThread(const Handle<Framebuffer> &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    Array<ID<Entity>> removed_proxies;
    proxy_list.GetRemovedEntities(removed_proxies);

    Array<RenderProxy *> added_proxies_ptrs;
    proxy_list.GetAddedEntities(added_proxies_ptrs, true);

    if (added_proxies_ptrs.Any() || removed_proxies.Any()) {
        Array<RenderProxy> added_proxies;
        added_proxies.Resize(added_proxies_ptrs.Size());

        for (uint i = 0; i < added_proxies_ptrs.Size(); i++) {
            AssertThrow(added_proxies_ptrs[i] != nullptr);

            // Copy the proxy to be added on the render thread
            added_proxies[i] = *added_proxies_ptrs[i];
        }

        PUSH_RENDER_COMMAND(
            RebuildProxyGroups,
            m_draw_collection,
            std::move(added_proxies),
            std::move(removed_proxies),
            framebuffer,
            override_attributes
        );
    }

    proxy_list.Advance(RPLAA_CLEAR);
}

void RenderList::PushEntityToRender(
    ID<Entity> entity,
    const RenderProxy &proxy
)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(entity.IsValid());
    AssertThrow(proxy.mesh.IsValid());

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);
    proxy_list.Add(entity, proxy);
}

void RenderList::CollectDrawCalls(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data
)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = ArrayMap<RenderableAttributeSet, RenderProxyGroup>::Iterator;

    Array<IteratorType> iterators;

    for (auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (auto &it : proxy_groups) {
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
            [this](IteratorType it, uint, uint)
            {
                RenderProxyGroup &proxy_group = it->second;

                Handle<RenderGroup> &render_group = proxy_group.render_group;
                AssertThrow(render_group.IsValid());

                render_group->CollectDrawCalls(proxy_group.render_proxies);
            }
        );
    } else {
        for (IteratorType it : iterators) {
            RenderProxyGroup &proxy_group = it->second;

            Handle<RenderGroup> &render_group = proxy_group.render_group;
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls(proxy_group.render_proxies);
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

    for (const auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : proxy_groups) {
            const RenderableAttributeSet &attributes = it.first;
            const RenderProxyGroup &proxy_group = it.second;

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            if (!bucket_bits.Test(uint(bucket))) {
                continue;
            }

            AssertThrow(proxy_group.render_group.IsValid());

            if (framebuffer) {
                AssertThrowMsg(
                    attributes.GetFramebufferID() == framebuffer->GetID(),
                    "Given Framebuffer's ID does not match RenderList item's framebuffer ID -- invalid data passed?"
                );
            }

            if (push_constant) {
                proxy_group.render_group->GetPipeline()->SetPushConstants(push_constant.ptr, push_constant.size);
            }

            if (use_draw_indirect && cull_data != nullptr) {
                proxy_group.render_group->PerformRenderingIndirect(frame);
            } else {
                proxy_group.render_group->PerformRendering(frame);
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

    using IteratorType = ArrayMap<RenderableAttributeSet, RenderProxyGroup>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : proxy_groups) {
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
        const RenderProxyGroup &proxy_group = it.second;

        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

        if (!bucket_bits.Test(uint(bucket))) {
            continue;
        }

        AssertThrow(proxy_group.render_group.IsValid());

        if (framebuffer) {
            AssertThrowMsg(
                attributes.GetFramebufferID() == framebuffer->GetID(),
                "Given Framebuffer's ID does not match RenderList item's framebuffer ID -- invalid data passed?"
            );
        }

        if (push_constant) {
            proxy_group.render_group->GetPipeline()->SetPushConstants(push_constant.ptr, push_constant.size);
        }

        if (use_draw_indirect && cull_data != nullptr) {
            proxy_group.render_group->PerformRenderingIndirect(frame);
        } else {
            proxy_group.render_group->PerformRendering(frame);
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