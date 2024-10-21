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
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
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

// @TODO: Make Material, Texture(?), etc. only need to have their render objects (e.g DescriptorSetRef)
// while the objects are used by this system (any RenderProxy objects)
// would free up quite a bit of memory.
struct RENDER_COMMAND(RebuildProxyGroups) : renderer::RenderCommand
{
    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;
    FlatMap<ID<Entity>, RenderProxy>    changed_proxies;

    FramebufferRef                      framebuffer;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        FlatMap<ID<Entity>, RenderProxy> &&changed_proxies,
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        changed_proxies(std::move(changed_proxies)),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
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

        if (framebuffer != nullptr) {
            attributes.SetFramebuffer(framebuffer);
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

        return attributes;
    }

    void AddRenderProxy(RenderProxyList &proxy_list, const RenderProxy &proxy, const RenderableAttributeSet &attributes, PassType pass_type)
    {
        HYP_SCOPE;

        const ID<Entity> entity = proxy.entity.GetID();

        // Add proxy to group
        RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];

        if (!render_proxy_group.GetRenderGroup().IsValid()) {
            HYP_LOG(RenderCollection, LogLevel::DEBUG, "Creating RenderGroup for attributes:\n"
                "\tVertex Attributes: {}\n"
                "\tBucket: {}\n"
                "\tShader Definition: {}  {}\n"
                "\tFramebuffer: {}\n"
                "\tCull Mode: {}\n"
                "\tFlags: {}\n"
                "\tDrawable layer: {}",
                attributes.GetMeshAttributes().vertex_attributes.flag_mask,
                uint32(attributes.GetMaterialAttributes().bucket),
                attributes.GetShaderDefinition().GetName(),
                attributes.GetShaderDefinition().GetProperties().GetHashCode().Value(),
                attributes.GetFramebuffer().index,
                uint32(attributes.GetMaterialAttributes().cull_faces),
                uint32(attributes.GetMaterialAttributes().flags),
                attributes.GetDrawableLayer());

            // Create RenderGroup
            Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
                g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                attributes
            );

            InitObject(render_group);

            render_proxy_group.SetRenderGroup(render_group);
        }

        render_proxy_group.AddRenderProxy(proxy);

        proxy_list.Add(entity, std::move(proxy));
    }

    bool RemoveRenderProxy(RenderProxyList &proxy_list, ID<Entity> entity, const RenderableAttributeSet &attributes, PassType pass_type)
    {
        HYP_SCOPE;

        bool removed = false;

        // for (auto &proxy_groups : collection->GetProxyGroups()) {
            // for (auto &it : proxy_groups) {
            //     removed |= it.second.RemoveRenderProxy(entity);
            // }

            auto &proxy_groups = collection->GetProxyGroups()[pass_type];

            auto it = proxy_groups.Find(attributes);
            AssertThrow(it != proxy_groups.End());

            removed |= it->second.RemoveRenderProxy(entity);
        // }

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

            const Handle<Mesh> &mesh = proxy->mesh;
            AssertThrow(mesh.IsValid());

            const Handle<Material> &material = proxy->material;
            AssertThrow(material.IsValid());

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy);
            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

            AssertThrow(RemoveRenderProxy(proxy_list, entity, attributes, pass_type));
        }

        HYP_LOG(RenderCollection, LogLevel::DEBUG, "Added Proxies: {}\nRemoved Proxies: {}\nChanged Proxies:{}",
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

#pragma region EntityList

RenderProxyGroup::RenderProxyGroup(const RenderProxyGroup &other)
    : m_render_proxies(other.m_render_proxies),
      m_render_group(other.m_render_group)
{
}

RenderProxyGroup &RenderProxyGroup::operator=(const RenderProxyGroup &other)
{
    if (this == &other) {
        return *this;
    }

    m_render_proxies = other.m_render_proxies;
    m_render_group = other.m_render_group;

    return *this;
}

RenderProxyGroup::RenderProxyGroup(RenderProxyGroup &&other) noexcept
    : m_render_proxies(std::move(other.m_render_proxies)),
      m_render_group(std::move(other.m_render_group))
{
}

RenderProxyGroup &RenderProxyGroup::operator=(RenderProxyGroup &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_render_proxies = std::move(other.m_render_proxies);
    m_render_group = std::move(other.m_render_group);

    return *this;
}

void RenderProxyGroup::ClearProxies()
{
    m_render_proxies.Clear();
}

void RenderProxyGroup::AddRenderProxy(const RenderProxy &render_proxy)
{
    m_render_proxies.Insert(Pair<ID<Entity>, RenderProxy> { render_proxy.entity.GetID(), render_proxy });
}

bool RenderProxyGroup::RemoveRenderProxy(ID<Entity> entity)
{
    const auto it = m_render_proxies.Find(entity);

    if (it == m_render_proxies.End()) {
        return false;
    }

    m_render_proxies.Erase(it);

    return true;
}

typename FlatMap<ID<Entity>, RenderProxy>::Iterator RenderProxyGroup::RemoveRenderProxy(typename FlatMap<ID<Entity>, RenderProxy>::ConstIterator iterator)
{
    return m_render_proxies.Erase(iterator);
}

void RenderProxyGroup::ResetRenderGroup()
{
    m_render_group.Reset();
}

void RenderProxyGroup::SetRenderGroup(const Handle<RenderGroup> &render_group)
{
    m_render_group = render_group;
}

#pragma endregion EntityList

#pragma region EntityDrawCollection

FixedArray<FlatMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &EntityDrawCollection::GetProxyGroups()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    return m_proxy_groups;
}

const FixedArray<FlatMap<RenderableAttributeSet, RenderProxyGroup>, PASS_TYPE_MAX> &EntityDrawCollection::GetProxyGroups() const
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

void EntityDrawCollection::ClearProxyGroups(bool reset_render_groups)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &proxy_groups : GetProxyGroups()) {
        for (auto &it : proxy_groups) {
            it.second.ClearProxies();

            if (reset_render_groups) {
                it.second.ResetRenderGroup();
            }
        }
    }
}

void EntityDrawCollection::RemoveEmptyProxyGroups()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    for (auto &proxy_groups : GetProxyGroups()) {
        for (auto it = proxy_groups.Begin(); it != proxy_groups.End();) {
            if (it->second.GetRenderProxies().Any()) {
                ++it;

                continue;
            }

            it = proxy_groups.Erase(it);
        }
    }
}

uint32 EntityDrawCollection::NumRenderGroups() const
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    uint32 count = 0;

    for (const auto &pass_type_proxy_groups : m_proxy_groups) {
        for (const auto &it : pass_type_proxy_groups) {
            if (it.second.m_render_group.IsValid()) {
                ++count;
            }
        }
    }

    return count;
}

#pragma endregion EntityDrawCollection

#pragma region RenderList

RenderList::RenderList()
    : m_render_environment(nullptr),
      m_draw_collection(MakeRefCountedPtr<EntityDrawCollection>())
{
}

RenderList::RenderList(const Handle<Camera> &camera)
    : m_render_environment(nullptr),
      m_camera(camera)
{
}

RenderList::~RenderList()
{
}

RenderListCollectionResult RenderList::PushUpdatesToRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);
    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    RenderListCollectionResult collection_result { };
    collection_result.num_added_entities = proxy_list.GetAddedEntities().Count();
    collection_result.num_removed_entities = proxy_list.GetRemovedEntities().Count();
    collection_result.num_changed_entities = proxy_list.GetChangedEntities().Count();

    if (collection_result.NeedsUpdate()) {
        Array<ID<Entity>> removed_proxies;
        proxy_list.GetRemovedEntities(removed_proxies);

        Array<RenderProxy *> added_proxies_ptrs;
        proxy_list.GetAddedEntities(added_proxies_ptrs, true /* include_changed */);

        FlatMap<ID<Entity>, RenderProxy> changed_proxies = std::move(proxy_list.GetChangedRenderProxies());

        if (added_proxies_ptrs.Any() || removed_proxies.Any() || changed_proxies.Any()) {
            Array<RenderProxy> added_proxies;
            added_proxies.Resize(added_proxies_ptrs.Size());

            for (SizeType i = 0; i < added_proxies_ptrs.Size(); i++) {
                AssertThrow(added_proxies_ptrs[i] != nullptr);

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

void RenderList::PushEntityToRender(
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

void RenderList::CollectDrawCalls(
    Frame *frame,
    const Bitset &bucket_bits,
    const CullData *cull_data
)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = FlatMap<RenderableAttributeSet, RenderProxyGroup>::Iterator;

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

    if (do_parallel_collection && iterators.Size() > 1) {
        TaskSystem::GetInstance().ParallelForEach(
            TaskThreadPoolName::THREAD_POOL_RENDER,
            iterators,
            [this](IteratorType it, uint, uint)
            {
                RenderProxyGroup &proxy_group = it->second;

                const Handle<RenderGroup> &render_group = proxy_group.GetRenderGroup();
                AssertThrow(render_group.IsValid());

                render_group->CollectDrawCalls(proxy_group.GetRenderProxies());
            }
        );
    } else {
        for (IteratorType it : iterators) {
            RenderProxyGroup &proxy_group = it->second;

            const Handle<RenderGroup> &render_group = proxy_group.GetRenderGroup();
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls(proxy_group.GetRenderProxies());
        }
    }

    if (use_draw_indirect && cull_data != nullptr) {
        for (SizeType index = 0; index < iterators.Size(); index++) {
            (*iterators[index]).second.GetRenderGroup()->PerformOcclusionCulling(frame, cull_data);
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
    const FramebufferRef &framebuffer,
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

    for (const auto &proxy_groups : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : proxy_groups) {
            const RenderableAttributeSet &attributes = it.first;
            const RenderProxyGroup &proxy_group = it.second;

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            if (!bucket_bits.Test(uint(bucket))) {
                continue;
            }

            AssertThrow(proxy_group.GetRenderGroup().IsValid());

            if (framebuffer.IsValid()) {
                AssertThrowMsg(
                    attributes.GetFramebuffer() == framebuffer,
                    "Given Framebuffer does not match RenderList item's framebuffer -- invalid data passed?"
                );
            }

            if (push_constant) {
                proxy_group.GetRenderGroup()->GetPipeline()->SetPushConstants(push_constant.Data(), push_constant.Size());
            }

            if (use_draw_indirect && cull_data != nullptr) {
                proxy_group.GetRenderGroup()->PerformRenderingIndirect(frame);
            } else {
                proxy_group.GetRenderGroup()->PerformRendering(frame);
            }
        }
    }

    g_engine->GetRenderState().UnbindCamera();

    if (framebuffer) {
        framebuffer->EndCapture(command_buffer, frame_index);
    }
}

void RenderList::Reset()
{
    HYP_NAMED_SCOPE("RenderList Reset");

    AssertThrow(m_draw_collection != nullptr);
    // Threads::AssertOnThread(ThreadName::THREAD_GAME);

    // perform full clear
    *m_draw_collection = { };
}

#pragma endregion RenderList

} // namespace hyperion