/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/system/AppContext.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <util/fs/FsUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);


struct alignas(16) EntityInstanceBatch_UI : EntityInstanceBatch
{
    Vec4f   texcoords[max_entities_per_instance_batch];
    Vec4f   offsets[max_entities_per_instance_batch];
    Vec4f   sizes[max_entities_per_instance_batch];
};

static_assert(sizeof(EntityInstanceBatch_UI) == 6976);

static constexpr uint32 max_ui_entity_instance_batches = 16384;

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups_UI) : renderer::RenderCommand
{
    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;
    RenderProxyEntityMap                changed_proxies;

    Array<Pair<ID<Entity>, int>>        proxy_depths;

    FramebufferRef                      framebuffer;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy> &&added_proxies,
        Array<ID<Entity>> &&removed_proxies,
        RenderProxyEntityMap &&changed_proxies,
        const Array<Pair<ID<Entity>, int>> &proxy_depths,
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        added_proxies(std::move(added_proxies)),
        removed_proxies(std::move(removed_proxies)),
        changed_proxies(std::move(changed_proxies)),
        proxy_depths(proxy_depths),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override
    {
        SafeRelease(std::move(framebuffer));
    }

    RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet &entity_attributes) const
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

        RenderableAttributeSet attributes = entity_attributes;

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

    void BuildProxyGroupsInOrder()
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: BuildProxyGroupsInOrder");

        collection->ClearProxyGroups();
        
        struct
        {
            HashCode    attributes_hash_code;
            uint32      drawable_layer = ~0u;
        } last_render_proxy_group;

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (const Pair<ID<Entity>, int> &pair : proxy_depths) {
            RenderProxy *proxy = proxy_list.GetProxyForEntity(pair.first);

            if (!proxy) {
                continue;
            }

            const Handle<Mesh> &mesh = proxy->mesh;
            const Handle<Material> &material = proxy->material;

            if (!mesh.IsValid() || !material.IsValid()) {
                continue;
            }

            RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet {
                mesh->GetMeshAttributes(),
                material->GetRenderAttributes()
            });

            const PassType pass_type = BucketToPassType(attributes.GetMaterialAttributes().bucket);

            // skip non-UI items
            if (pass_type != PASS_TYPE_UI) {
                continue;
            }
            
            attributes.SetDrawableLayer(pair.second);

            Handle<RenderGroup> &render_group = collection->GetProxyGroups()[pass_type][attributes];

            if (!render_group.IsValid()) {
                // Create RenderGroup
                render_group = CreateObject<RenderGroup>(
                    g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                    attributes,
                    RenderGroupFlags::DEFAULT & ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING)
                );

                render_group->SetDrawCallCollectionImpl(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch_UI>(max_ui_entity_instance_batches));

                if (framebuffer != nullptr) {
                    render_group->AddFramebuffer(framebuffer);
                } else {
                    FramebufferRef bucket_framebuffer = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(attributes.GetMaterialAttributes().bucket).GetFramebuffer();
                    AssertThrow(bucket_framebuffer != nullptr);

                    render_group->AddFramebuffer(bucket_framebuffer);
                }

                HYP_LOG(UI, Debug, "Create render group {} (#{})", attributes.GetHashCode().Value(), render_group.GetID().Value());

#ifdef HYP_DEBUG_MODE
                if (!render_group.IsValid()) {
                    HYP_LOG(UI, Error, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                    continue;
                }
#endif

                InitObject(render_group);
            }

            render_group->AddRenderProxy(*proxy);
        }

        collection->RemoveEmptyProxyGroups();
    }

    bool RemoveRenderProxy(RenderProxyList &proxy_list, ID<Entity> entity)
    {
        HYP_SCOPE;

        bool removed = false;

        for (auto &render_groups_by_attributes : collection->GetProxyGroups()) {
            for (auto &it : render_groups_by_attributes) {
                removed |= it.second->RemoveRenderProxy(entity);
            }
        }

        proxy_list.MarkToRemove(entity);

        return removed;
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups");

        RenderProxyList &proxy_list = collection->GetProxyList(ThreadType::THREAD_TYPE_RENDER);

        for (const auto &it : changed_proxies) {
            const ID<Entity> entity = it.first;
            const RenderProxy &proxy = it.second;

            proxy.UnclaimRenderResources();

            // Remove it, then add it back (changed proxies will be included in the added proxies list)
            AssertThrow(RemoveRenderProxy(proxy_list, entity));
        }

        for (RenderProxy &proxy : added_proxies) {
            proxy.ClaimRenderResources();

            proxy_list.Add(proxy.entity.GetID(), std::move(proxy));
        }

        for (ID<Entity> entity : removed_proxies) {
            const RenderProxy *proxy = proxy_list.GetProxyForEntity(entity);
            AssertThrow(proxy != nullptr);

            proxy->UnclaimRenderResources();

            proxy_list.MarkToRemove(entity);
        }

        proxy_list.Advance(RenderProxyListAdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateUIRendererFramebuffer) : renderer::RenderCommand
{
    RC<UIRenderer>  ui_renderer;

    RENDER_COMMAND(CreateUIRendererFramebuffer)(RC<UIRenderer> ui_renderer)
        : ui_renderer(ui_renderer)
    {
        AssertThrow(ui_renderer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateUIRendererFramebuffer)() override = default;

    virtual RendererResult operator()() override
    {
        ui_renderer->CreateFramebuffer();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

UIRenderCollector::UIRenderCollector()
    : RenderCollector()
{
}

UIRenderCollector::UIRenderCollector(const Handle<Camera> &camera)
    : RenderCollector(camera)
{
}

UIRenderCollector::~UIRenderCollector() = default;

void UIRenderCollector::ResetOrdering()
{
    m_proxy_depths.Clear();
}

void UIRenderCollector::PushEntityToRender(ID<Entity> entity, const RenderProxy &proxy, int computed_depth)
{
    RenderCollector::PushEntityToRender(entity, proxy);

    m_proxy_depths.EmplaceBack(entity, computed_depth);
}

RenderCollector::CollectionResult UIRenderCollector::PushUpdatesToRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);
    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    RenderCollector::CollectionResult collection_result { };
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
                AssertThrow(added_proxies_ptrs[i] != nullptr);

                // Copy the proxy to be added on the render thread
                added_proxies[i] = *added_proxies_ptrs[i];
            }

            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                m_draw_collection,
                std::move(added_proxies),
                std::move(removed_proxies),
                std::move(changed_proxies),
                m_proxy_depths,
                framebuffer,
                override_attributes
            );
        }
    }

    proxy_list.Advance(RenderProxyListAdvanceAction::CLEAR);

    return collection_result;
}

void UIRenderCollector::CollectDrawCalls(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::Iterator;

    Array<IteratorType> iterators;

    for (auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
        for (auto &it : render_groups_by_attributes) {
            const RenderableAttributeSet &attributes = it.first;

            if (attributes.GetMaterialAttributes().bucket != BUCKET_UI) {
                continue;
            }

            iterators.PushBack(&it);
        }
    }

    TaskSystem::GetInstance().ParallelForEach(
        TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER),
        iterators,
        [this](IteratorType it, uint32, uint32)
        {
            Handle<RenderGroup> &render_group = it->second;
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls();
        }
    );
}

void UIRenderCollector::ExecuteDrawCalls(Frame *frame) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    AssertThrow(m_draw_collection != nullptr);

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint32 frame_index = frame->GetFrameIndex();

    AssertThrowMsg(m_camera.IsValid(), "Cannot render with invalid Camera");

    const FramebufferRef &framebuffer = m_camera->GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    framebuffer->BeginCapture(command_buffer, frame_index);

    g_engine->GetRenderState()->BindCamera(m_camera.Get());

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
        for (const auto &it : render_groups_by_attributes) {
            iterators.PushBack(&it);
        }
    }

    {
        HYP_NAMED_SCOPE("Sort proxy groups by layer");

        std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
        {
            return lhs->first.GetDrawableLayer() < rhs->first.GetDrawableLayer();
        });
    }

    // HYP_LOG(UI, Debug, " UI rendering {} render groups", iterators.Size());

    for (SizeType index = 0; index < iterators.Size(); index++) {
        const auto &it = *iterators[index];

        const RenderableAttributeSet &attributes = it.first;
        const Handle<RenderGroup> &render_group = it.second;

        if (attributes.GetMaterialAttributes().bucket != BUCKET_UI) {
            continue;
        }

        AssertThrow(render_group.IsValid());

        render_group->PerformRendering(frame);
    }

    g_engine->GetRenderState()->UnbindCamera(m_camera.Get());

    framebuffer->EndCapture(command_buffer, frame_index);
}

#pragma endregion UIRenderCollector

#pragma region UIRenderer

UIRenderer::UIRenderer(Name name, RC<UIStage> ui_stage)
    : RenderSubsystem(name),
      m_ui_stage(std::move(ui_stage))
{
}

UIRenderer::~UIRenderer()
{
    SafeRelease(std::move(m_framebuffer));
    g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::Init()
{
    HYP_SCOPE;

    m_on_gbuffer_resolution_changed_handle = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([this]()
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        SafeRelease(std::move(m_framebuffer));
        g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);

        CreateFramebuffer();
    });

    PUSH_RENDER_COMMAND(CreateUIRendererFramebuffer, RefCountedPtrFromThis().CastUnsafe<UIRenderer>());

    AssertThrow(m_ui_stage != nullptr);

    AssertThrow(m_ui_stage->GetScene() != nullptr);
    AssertThrow(m_ui_stage->GetScene()->GetCamera().IsValid());

    m_render_collector.SetCamera(m_ui_stage->GetScene()->GetCamera());
}

void UIRenderer::CreateFramebuffer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const Vec2i surface_size = Vec2i(g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

    m_ui_stage->GetScene()->GetEntityManager()->PushCommand([ui_stage = m_ui_stage, surface_size](EntityManager &mgr, GameCounter::TickUnit delta)
    {
        ui_stage->SetSurfaceSize(surface_size);
    });

    m_framebuffer = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(Bucket::BUCKET_UI).GetFramebuffer();
    AssertThrow(m_framebuffer.IsValid());

    m_ui_stage->GetScene()->GetCamera()->SetFramebuffer(m_framebuffer);

    g_engine->GetFinalPass()->SetUITexture(CreateObject<Texture>(
        m_framebuffer->GetAttachment(0)->GetImage(),
        m_framebuffer->GetAttachment(0)->GetImageView()
    ));
}

// called from game thread
void UIRenderer::InitGame() { }

void UIRenderer::OnRemoved()
{
    g_engine->GetFinalPass()->SetUITexture(Handle<Texture>::empty);
}

void UIRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    m_render_collector.ResetOrdering();

    m_ui_stage->CollectObjects([this](UIObject *ui_object)
    {
        AssertThrow(ui_object != nullptr);

        const NodeProxy &node = ui_object->GetNode();
        AssertThrow(node.IsValid());

        const Handle<Entity> &entity = node->GetEntity();

        MeshComponent *mesh_component = node->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
        AssertThrow(mesh_component != nullptr);
        AssertThrow(mesh_component->proxy != nullptr);

        m_render_collector.PushEntityToRender(entity, *mesh_component->proxy, ui_object->GetComputedDepth());
    }, /* only_visible */ true);

    m_render_collector.PushUpdatesToRenderThread(m_ui_stage->GetScene()->GetCamera()->GetFramebuffer());
}

void UIRenderer::OnRender(Frame *frame)
{
    HYP_SCOPE;

    g_engine->GetRenderState()->BindScene(m_ui_stage->GetScene());

    m_render_collector.CollectDrawCalls(frame);
    m_render_collector.ExecuteDrawCalls(frame);

    g_engine->GetRenderState()->UnbindScene(m_ui_stage->GetScene());
}

#pragma endregion UIRenderer

} // namespace hyperion
