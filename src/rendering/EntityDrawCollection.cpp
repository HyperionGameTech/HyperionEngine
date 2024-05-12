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
    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;

    FramebufferRef                      framebuffer;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups)() override
    {
        SafeRelease(std::move(framebuffer));
    }

    virtual Result operator()() override
    {
        // Clear out all proxy groups and recreate them

        // @FIXME Remove this -- causing issues, but we need a way to tell what objects were removed
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

            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);
            
            // Add proxy to group
            RenderProxyGroup &render_proxy_group = collection->GetProxyGroups()[pass_type][attributes];

            if (!render_proxy_group.GetRenderGroup().IsValid()) {
                DebugLog(LogType::Debug, "Creating RenderGroup for attributes:\n"
                    "\tVertex Attributes: %llu\n"
                    "\tBucket: %u\n"
                    "\tShader Definition: %s  %llu\n"
                    "\tFramebuffer: %llu\n"
                    "\tCull Mode: %u\n"
                    "\tFlags: %u\n"
                    "\tDrawable layer: %u\n"
                    "\tMesh name: %s\n"
                    "\n",
                    attributes.GetMeshAttributes().vertex_attributes.flag_mask,
                    attributes.GetMaterialAttributes().bucket,
                    *attributes.GetShaderDefinition().GetName(),
                    attributes.GetShaderDefinition().GetProperties().GetHashCode().Value(),
                    attributes.GetFramebuffer().index,
                    attributes.GetMaterialAttributes().cull_faces,
                    attributes.GetMaterialAttributes().flags,
                    attributes.GetDrawableLayer().layer_index,
                    mesh.IsValid() ? *mesh->GetName() : "<None>");

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

                render_proxy_group.SetRenderGroup(render_group);
            }

            render_proxy_group.AddRenderProxy(proxy);

            proxy_list.Add(entity, std::move(proxy));
        }

        for (const ID<Entity> &entity : removed_proxies) {
            proxy_list.MarkToRemove(entity);
        }

        proxy_list.Advance(RPLAA_PERSIST);
        
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
    m_render_proxies.PushBack(render_proxy);
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

void EntityDrawCollection::AddRenderProxy(ThreadType thread_type, const RenderableAttributeSet &attributes, const RenderProxy &proxy)
{
    auto &group = GetProxyGroups()[BucketToPassType(attributes.GetMaterialAttributes().bucket)][attributes];

    group.AddRenderProxy(proxy);
}

void EntityDrawCollection::AddRenderProxy(const RenderableAttributeSet &attributes, const RenderProxy &proxy)
{
    AddRenderProxy(Threads::GetThreadType(), attributes, proxy);
}

void EntityDrawCollection::ClearProxyGroups(bool reset_render_groups)
{
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

void RenderList::UpdateOnRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    AssertThrowMsg(camera.IsValid(), "Cannot render with invalid Camera");

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    if (framebuffer) {
        framebuffer->BeginCapture(command_buffer, frame_index);
    }

    g_engine->GetRenderState().BindCamera(camera.Get());

    uint32 num_rendered_proxies = 0;

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

            num_rendered_proxies += it.second.GetRenderProxies().Size();
        }
    }

    DebugLog(LogType::Debug, "Rendered %u proxies\n", num_rendered_proxies);

    g_engine->GetRenderState().UnbindCamera();

    if (framebuffer) {
        framebuffer->EndCapture(command_buffer, frame_index);
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