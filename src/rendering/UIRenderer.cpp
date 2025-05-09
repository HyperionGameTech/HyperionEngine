/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/EngineRenderStats.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <scene/Mesh.hpp>
#include <scene/Texture.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

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
    Array<ID<Entity>>                   removed_entities;

    Array<Pair<ID<Entity>, int>>        proxy_depths;

    FramebufferRef                      framebuffer;

    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy *> &&added_proxy_ptrs,
        Array<ID<Entity>> &&removed_entities,
        const Array<Pair<ID<Entity>, int>> &proxy_depths,
        const FramebufferRef &framebuffer,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : collection(collection),
        removed_entities(std::move(removed_entities)),
        proxy_depths(proxy_depths),
        framebuffer(framebuffer),
        override_attributes(override_attributes)
    {
        added_proxies.Reserve(added_proxy_ptrs.Size());

        for (RenderProxy *proxy_ptr : added_proxy_ptrs) {
            added_proxies.PushBack(*proxy_ptr);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override = default;

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

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;
            
            attributes.SetDrawableLayer(pair.second);

            Handle<RenderGroup> &render_group = collection->GetProxyGroups()[uint32(bucket)][attributes];

            if (!render_group.IsValid()) {
                // Create RenderGroup
                render_group = CreateObject<RenderGroup>(
                    g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                    attributes,
                    RenderGroupFlags::DEFAULT & ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING)
                );

                render_group->SetDrawCallCollectionImpl(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch_UI>(max_ui_entity_instance_batches));

                render_group->AddFramebuffer(framebuffer);

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

        // Reserve to prevent iterator invalidation
        proxy_list.Reserve(added_proxies.Size());

        // Claim before unclaiming items from removed_entities so modified proxies (which would be in removed_entities)
        // don't have their resources destroyed unnecessarily, causing destroy + recreate to occur much too frequently.
        for (RenderProxy &proxy : added_proxies) {
            proxy.ClaimRenderResource();
        }

        for (ID<Entity> entity : removed_entities) {
            const RenderProxy *proxy = proxy_list.GetProxyForEntity(entity);
            AssertThrow(proxy != nullptr);

            proxy->UnclaimRenderResource();

            proxy_list.MarkToRemove(entity);
        }

        for (RenderProxy &proxy : added_proxies) {
            proxy_list.Add(proxy.entity.GetID(), std::move(proxy));
        }

        proxy_list.Advance(RenderProxyListAdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

UIRenderCollector::UIRenderCollector()
    : RenderCollector()
{
}

UIRenderCollector::~UIRenderCollector() = default;

void UIRenderCollector::ResetOrdering()
{
    m_proxy_depths.Clear();
}

void UIRenderCollector::PushRenderProxy(RenderProxyList &proxy_list, const RenderProxy &render_proxy, int computed_depth)
{
    RenderCollector::PushRenderProxy(proxy_list, render_proxy);

    m_proxy_depths.EmplaceBack(render_proxy.entity.GetID(), computed_depth);
}

RenderCollector::CollectionResult UIRenderCollector::PushUpdatesToRenderThread(const FramebufferRef &framebuffer, const Optional<RenderableAttributeSet> &override_attributes)
{
    HYP_SCOPE;

    // UISubsystem can have Update() called on a task thread.
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertThrow(m_draw_collection != nullptr);

    RenderProxyList &proxy_list = m_draw_collection->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    RenderCollector::CollectionResult collection_result { };
    collection_result.num_added_entities = proxy_list.GetAddedEntities().Count();
    collection_result.num_removed_entities = proxy_list.GetRemovedEntities().Count();
    collection_result.num_changed_entities = proxy_list.GetChangedEntities().Count();

    if (collection_result.NeedsUpdate()) {
        Array<ID<Entity>> removed_entities;
        proxy_list.GetRemovedEntities(removed_entities, true /* include_changed */);

        Array<RenderProxy *> added_proxies_ptrs;
        proxy_list.GetAddedEntities(added_proxies_ptrs, true /* include_changed */);

        if (added_proxies_ptrs.Any() || removed_entities.Any()) {
            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                m_draw_collection,
                std::move(added_proxies_ptrs),
                std::move(removed_entities),
                m_proxy_depths,
                framebuffer,
                override_attributes
            );
        }
    }

    proxy_list.Advance(RenderProxyListAdvanceAction::CLEAR);

    return collection_result;
}

void UIRenderCollector::CollectDrawCalls(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::Iterator;

    Array<IteratorType> iterators;

    for (auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
        for (auto &it : render_groups_by_attributes) {
            const RenderableAttributeSet &attributes = it.first;

            iterators.PushBack(&it);
        }
    }

    TaskSystem::GetInstance().ParallelForEach(
        HYP_STATIC_MESSAGE("UIRenderCollector::CollectDrawCalls"),
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

void UIRenderCollector::ExecuteDrawCalls(
    FrameBase *frame, 
    ViewRenderResource *view,
    const TResourceHandle<CameraRenderResource> &camera_resource_handle,
    const FramebufferRef &framebuffer
) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
    
    AssertThrow(m_draw_collection != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    if (framebuffer.IsValid()) {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
    }

    g_engine->GetRenderState()->BindCamera(camera_resource_handle);

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

        AssertThrow(render_group.IsValid());

        // Don't count draw calls for UI
        SuppressEngineRenderStatsScope suppress_render_stats_scope;

        render_group->PerformRendering(frame, view);
    }

    g_engine->GetRenderState()->UnbindCamera(camera_resource_handle.Get());
    
    if (framebuffer.IsValid()) {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frame_index);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIRenderSubsystem

UIRenderSubsystem::UIRenderSubsystem(Name name, const RC<UIStage> &ui_stage)
    : RenderSubsystem(name),
      m_ui_stage(ui_stage)
{
}

UIRenderSubsystem::~UIRenderSubsystem()
{
    g_engine->GetFinalPass()->SetUILayerImageView(g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
}

void UIRenderSubsystem::Init()
{
    HYP_SCOPE;

    struct RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer) : renderer::RenderCommand
    {
        RC<UIRenderSubsystem>   ui_render_subsystem;

        RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)(const RC<UIRenderSubsystem> &ui_render_subsystem)
            : ui_render_subsystem(ui_render_subsystem)
        {
            AssertThrow(ui_render_subsystem != nullptr);
        }

        virtual ~RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)() override = default;

        virtual RendererResult operator()() override
        {
            ui_render_subsystem->CreateFramebuffer();

            HYPERION_RETURN_OK;
        }
    };

    m_on_gbuffer_resolution_changed_handle = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([weak_this = WeakRefCountedPtrFromThis()]()
    {
        Threads::AssertOnThread(g_render_thread);

        HYP_LOG(UI, Debug, "UIRenderSubsystem: resizing to {}", g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

        RC<UIRenderSubsystem> subsystem = weak_this.Lock().CastUnsafe<UIRenderSubsystem>();

        if (!subsystem) {
            HYP_LOG(UI, Warning, "UIRenderSubsystem: subsystem is expired on resize");

            return;
        }

        g_engine->GetFinalPass()->SetUILayerImageView(g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

        SafeRelease(std::move(subsystem->m_framebuffer));

        PUSH_RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer, subsystem);
    });

    AssertThrow(m_ui_stage != nullptr);
    AssertThrow(m_ui_stage->GetCamera().IsValid());
    AssertThrow(m_ui_stage->GetCamera()->IsReady());

    m_camera_resource_handle = TResourceHandle<CameraRenderResource>(m_ui_stage->GetCamera()->GetRenderResource());

    PUSH_RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer, RefCountedPtrFromThis().CastUnsafe<UIRenderSubsystem>());
}

// called from game thread
void UIRenderSubsystem::InitGame() { }

void UIRenderSubsystem::OnRemoved()
{
    SafeRelease(std::move(m_framebuffer));
    
    g_engine->GetFinalPass()->SetUILayerImageView(g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

    m_on_gbuffer_resolution_changed_handle.Reset();
}

void UIRenderSubsystem::OnUpdate(GameCounter::TickUnit delta)
{
    // do nothing
}

void UIRenderSubsystem::OnRender(FrameBase *frame)
{
    HYP_SCOPE;

    g_engine->GetRenderState()->SetActiveScene(m_ui_stage->GetScene());

    m_render_collector.CollectDrawCalls(frame);
    m_render_collector.ExecuteDrawCalls(frame, nullptr, m_camera_resource_handle, m_framebuffer);

    g_engine->GetRenderState()->UnsetActiveScene();
}

void UIRenderSubsystem::CreateFramebuffer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const Vec2i surface_size = g_engine->GetAppContext()->GetMainWindow()->GetDimensions();

    m_framebuffer = g_rendering_api->MakeFramebuffer(Vec2u(surface_size) * 2);

    AttachmentRef color_attachment = m_framebuffer->AddAttachment(
        0,
        InternalFormat::RGBA16F,
        ImageType::TEXTURE_TYPE_2D,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    m_framebuffer->AddAttachment(
        1,
        g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
        ImageType::TEXTURE_TYPE_2D,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    DeferCreate(m_framebuffer);

    m_camera_resource_handle->SetFramebuffer(m_framebuffer);

    m_ui_stage->GetScene()->GetEntityManager()->PushCommand([ui_stage = m_ui_stage, surface_size](EntityManager &mgr, GameCounter::TickUnit delta)
    {
        ui_stage->SetSurfaceSize(surface_size);
    });

    g_engine->GetFinalPass()->SetUILayerImageView(color_attachment->GetImageView());
}

#pragma endregion UIRenderSubsystem

} // namespace hyperion
