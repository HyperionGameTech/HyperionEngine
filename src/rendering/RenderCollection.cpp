/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/containers/Array.hpp>

#include <core/utilities/UniqueID.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Util.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/animation/Skeleton.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr bool do_parallel_collection = true;

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups) : renderer::RenderCommand
{
    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;
    RenderProxyEntityMap                changed_proxies;

    FramebufferRef                      framebuffer;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        RenderProxyEntityMap &&changed_proxies,
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        changed_proxies(std::move(changed_proxies)),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
        // sanity checking
        for (auto &it : this->added_proxies) {
            if (!it.material) {
                continue;
            }
            AssertThrow(it.material->GetRenderAttributes().bucket != BUCKET_UI);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups)() override
    {
        SafeRelease(std::move(framebuffer));
    }

    RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxy &proxy) const
    {
        HYP_SCOPE;
        
        const Handle<Mesh> &mesh = proxy.mesh;
        AssertThrow(mesh.IsValid());

        const Handle<Material> &material = proxy.material;
        AssertThrow(material.IsValid());

        RenderableAttributeSet attributes {
            mesh->GetMeshAttributes(),
            material->GetRenderAttributes()
        };

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

        return attributes;
    }

    void AddRenderProxy(RenderProxyList &proxy_list, const RenderProxy &proxy, const RenderableAttributeSet &attributes, PassType pass_type)
    {
        HYP_SCOPE;

        const ID<Entity> entity = proxy.entity.GetID();

        // Add proxy to group
        Handle<RenderGroup> &render_group = collection->GetProxyGroups()[pass_type][attributes];

        if (!render_group.IsValid()) {
            HYP_LOG(RenderCollection, LogLevel::DEBUG, "Creating RenderGroup for attributes:\n"
                "\tVertex Attributes: {}\n"
                "\tBucket: {}\n"
                "\tShader Definition: {}  {}\n"
                "\tCull Mode: {}\n"
                "\tFlags: {}\n"
                "\tDrawable layer: {}",
                attributes.GetMeshAttributes().vertex_attributes.flag_mask,
                uint32(attributes.GetMaterialAttributes().bucket),
                attributes.GetShaderDefinition().GetName(),
                attributes.GetShaderDefinition().GetProperties().GetHashCode().Value(),
                uint32(attributes.GetMaterialAttributes().cull_faces),
                uint32(attributes.GetMaterialAttributes().flags),
                attributes.GetDrawableLayer());

            // Create RenderGroup
            render_group = CreateObject<RenderGroup>(
                g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                attributes
            );

            if (framebuffer != nullptr) {
                render_group->AddFramebuffer(framebuffer);
            } else {
                const FramebufferRef &bucket_framebuffer = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(attributes.GetMaterialAttributes().bucket).GetFramebuffer();
                AssertThrow(bucket_framebuffer != nullptr);

                render_group->AddFramebuffer(bucket_framebuffer);
            }

            InitObject(render_group);
        }

        render_group->AddRenderProxy(proxy);

        proxy_list.Add(entity, std::move(proxy));
    }

    bool RemoveRenderProxy(RenderProxyList &proxy_list, ID<Entity> entity, const RenderableAttributeSet &attributes, PassType pass_type)
    {
        HYP_SCOPE;

        auto &render_groups_by_attributes = collection->GetProxyGroups()[pass_type];

        auto it = render_groups_by_attributes.Find(attributes);
        AssertThrow(it != render_groups_by_attributes.End());

        const Handle<RenderGroup> &render_group = it->second;
        AssertThrow(render_group.IsValid());

        const bool removed = render_group->RemoveRenderProxy(entity);

        proxy_list.MarkToRemove(entity);

        return removed;
    }

    virtual Result operator()() override
    {
        HYP_SCOPE;

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        // @TODO For changed proxies it would be more efficient
        // to update the RenderResources rather than Destroy + Recreate.

        for (const auto &it : changed_proxies) {
            const ID<Entity> entity = it.first;
            const RenderProxy &proxy = it.second;

            proxy.UnclaimRenderResources();

            const Handle<Mesh> &mesh = proxy.mesh;
            AssertThrow(mesh.IsValid());
            AssertThrow(mesh->IsReady());

            const Handle<Material> &material = proxy.material;
            AssertThrow(material.IsValid());

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(proxy);
            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);
            
            AssertThrow(RemoveRenderProxy(proxy_list, entity, attributes, pass_type));
        }

        for (const RenderProxy &proxy : added_proxies) {
            proxy.ClaimRenderResources();

            const Handle<Mesh> &mesh = proxy.mesh;
            AssertThrow(mesh.IsValid());
            AssertThrow(mesh->IsReady());

            const Handle<Material> &material = proxy.material;
            AssertThrow(material.IsValid());

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(proxy);
            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

            AddRenderProxy(proxy_list, proxy, attributes, pass_type);
        }

        for (ID<Entity> entity : removed_proxies) {
            const RenderProxy *proxy = proxy_list.GetProxyForEntity(entity);
            AssertThrow(proxy != nullptr);

            proxy->UnclaimRenderResources();

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy);
            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

            AssertThrow(RemoveRenderProxy(proxy_list, entity, attributes, pass_type));
        }

        HYP_LOG(RenderCollection, LogLevel::DEBUG, "Added Proxies: {}\nRemoved Proxies: {}\nChanged Proxies: {}",
            proxy_list.GetAddedEntities().Count(),
            proxy_list.GetRemovedEntities().Count(),
            proxy_list.GetChangedEntities().Count());

        proxy_list.Advance(RenderProxyListAdvanceAction::PERSIST);
        
        // Clear out groups that are no longer used
        collection->RemoveEmptyProxyGroups();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region EntityDrawCollection

FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, PASS_TYPE_MAX> &EntityDrawCollection::GetProxyGroups()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    return m_proxy_groups;
}

const FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, PASS_TYPE_MAX> &EntityDrawCollection::GetProxyGroups() const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    return m_proxy_groups;
}

RenderProxyList &EntityDrawCollection::GetProxyList(ThreadType thread_type)
{
    AssertThrowMsg(uint32(thread_type) <= ThreadType::THREAD_TYPE_RENDER, "Invalid thread for calling method");

    return m_proxy_lists[uint(thread_type)];
}

const RenderProxyList &EntityDrawCollection::GetProxyList(ThreadType thread_type) const
{
    AssertThrowMsg(uint32(thread_type) <= ThreadType::THREAD_TYPE_RENDER, "Invalid thread for calling method");

    return m_proxy_lists[uint(thread_type)];
}

void EntityDrawCollection::ClearProxyGroups()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &render_groups_by_attributes : GetProxyGroups()) {
        for (auto &it : render_groups_by_attributes) {
            it.second->ClearProxies();
        }
    }
}

void EntityDrawCollection::RemoveEmptyProxyGroups()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    for (auto &render_groups_by_attributes : GetProxyGroups()) {
        for (auto it = render_groups_by_attributes.Begin(); it != render_groups_by_attributes.End();) {
            if (it->second->GetRenderProxies().Any()) {
                ++it;

                continue;
            }

            it = render_groups_by_attributes.Erase(it);
        }
    }
}

uint32 EntityDrawCollection::NumRenderGroups() const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    uint32 count = 0;

    for (const auto &render_groups_by_attributes : m_proxy_groups) {
        for (const auto &it : render_groups_by_attributes) {
            if (it.second.IsValid()) {
                ++count;
            }
        }
    }

    return count;
}

#pragma endregion EntityDrawCollection

#pragma region RenderCollector

RenderCollector::RenderCollector()
    : m_draw_collection(MakeRefCountedPtr<EntityDrawCollection>())
{
}

RenderCollector::RenderCollector(const Handle<Camera> &camera)
    : m_camera(camera)
{
}

RenderCollector::~RenderCollector()
{
}

RenderCollector::CollectionResult RenderCollector::PushUpdatesToRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);
    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    CollectionResult collection_result { };
    collection_result.num_added_entities = proxy_list.GetAddedEntities().Count();
    collection_result.num_removed_entities = proxy_list.GetRemovedEntities().Count();
    collection_result.num_changed_entities = proxy_list.GetChangedEntities().Count();

    if (collection_result.NeedsUpdate()) {
        Array<ID<Entity>> removed_proxies;
        proxy_list.GetRemovedEntities(removed_proxies);

        Array<RenderProxy *> added_proxies_ptrs;
        proxy_list.GetAddedEntities(added_proxies_ptrs, true /* include_changed */);

        RenderProxyEntityMap changed_proxies = std::move(proxy_list.GetChangedRenderProxies());

        if (added_proxies_ptrs.Any() || removed_proxies.Any() || changed_proxies.Any()) {
            Array<RenderProxy> added_proxies;
            added_proxies.Resize(added_proxies_ptrs.Size());

            for (SizeType i = 0; i < added_proxies_ptrs.Size(); i++) {
                AssertDebug(added_proxies_ptrs[i] != nullptr);

                AssertDebug(added_proxies_ptrs[i]->mesh.IsValid());
                AssertDebug(added_proxies_ptrs[i]->mesh->IsInitCalled());

                AssertDebug(added_proxies_ptrs[i]->material.IsValid());
                AssertDebug(added_proxies_ptrs[i]->material->IsInitCalled());

                // Copy the proxy to be added on the render thread
                added_proxies[i] = *added_proxies_ptrs[i];
            }

            PUSH_RENDER_COMMAND(
                RebuildProxyGroups,
                m_draw_collection,
                std::move(added_proxies),
                std::move(removed_proxies),
                std::move(changed_proxies),
                framebuffer,
                override_attributes
            );
        }
    }

    proxy_list.Advance(RenderProxyListAdvanceAction::CLEAR);

    return collection_result;
}

void RenderCollector::PushEntityToRender(
    ID<Entity> entity,
    const RenderProxy &proxy
)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    AssertThrow(entity.IsValid());
    AssertThrow(proxy.mesh.IsValid());

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);
    proxy_list.Add(entity, proxy);
}

void RenderCollector::CollectDrawCalls(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data
)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::Iterator;

    Array<IteratorType> iterators;

    for (auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
        for (auto &it : render_groups_by_attributes) {
            const RenderableAttributeSet &attributes = it.first;

            const Bucket bucket = bucket_bits.Test(uint(attributes.GetMaterialAttributes().bucket)) ? attributes.GetMaterialAttributes().bucket : BUCKET_INVALID;

            if (bucket == BUCKET_INVALID) {
                continue;
            }

            iterators.PushBack(&it);
        }
    }

    if (do_parallel_collection && iterators.Size() > 1) {
        TaskSystem::GetInstance().ParallelForEach(
            TaskThreadPoolName::THREAD_POOL_RENDER,
            iterators,
            [this](IteratorType it, uint, uint)
            {
                Handle<RenderGroup> &render_group = it->second;
                AssertThrow(render_group.IsValid());

                render_group->CollectDrawCalls();
            }
        );
    } else {
        for (IteratorType it : iterators) {
            Handle<RenderGroup> &render_group = it->second;
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls();
        }
    }

    if (use_draw_indirect && cull_data != nullptr) {
        for (SizeType index = 0; index < iterators.Size(); index++) {
            (*iterators[index]).second->PerformOcclusionCulling(frame, cull_data);
        }
    }
}

void RenderCollector::ExecuteDrawCalls(
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

void RenderCollector::ExecuteDrawCalls(
    Frame *frame,
    const FramebufferRef &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(m_camera.IsValid());

    ExecuteDrawCalls(frame, m_camera, framebuffer, bucket_bits, cull_data, push_constant);
}

void RenderCollector::ExecuteDrawCalls(
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

void RenderCollector::ExecuteDrawCalls(
    Frame *frame,
    const Handle<Camera> &camera,
    const FramebufferRef &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    AssertThrowMsg(camera.IsValid(), "Cannot render with invalid Camera");

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    if (framebuffer) {
        framebuffer->BeginCapture(command_buffer, frame_index);
    }

    g_engine->GetRenderState().BindCamera(camera.Get());

    for (const auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : render_groups_by_attributes) {
            const RenderableAttributeSet &attributes = it.first;
            const Handle<RenderGroup> &render_group = it.second;

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            if (!bucket_bits.Test(uint(bucket))) {
                continue;
            }

            AssertThrow(render_group.IsValid());

            if (push_constant) {
                render_group->GetPipeline()->SetPushConstants(push_constant.Data(), push_constant.Size());
            }

            if (use_draw_indirect && cull_data != nullptr) {
                render_group->PerformRenderingIndirect(frame);
            } else {
                render_group->PerformRendering(frame);
            }
        }
    }

    g_engine->GetRenderState().UnbindCamera(camera.Get());

    if (framebuffer) {
        framebuffer->EndCapture(command_buffer, frame_index);
    }
}

void RenderCollector::Reset()
{
    HYP_NAMED_SCOPE("RenderCollector Reset");

    AssertThrow(m_draw_collection != nullptr);
    // Threads::AssertOnThread(ThreadName::THREAD_GAME);

    // perform full clear
    *m_draw_collection = { };
}

#pragma endregion RenderCollector

} // namespace hyperion