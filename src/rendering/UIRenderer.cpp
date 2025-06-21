/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/EngineRenderStats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/RenderTexture.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <scene/Mesh.hpp>
#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/World.hpp>

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

struct alignas(16) UIEntityInstanceBatch : EntityInstanceBatch
{
    Vec4f texcoords[max_entities_per_instance_batch];
    Vec4f offsets[max_entities_per_instance_batch];
    Vec4f sizes[max_entities_per_instance_batch];
};

static_assert(sizeof(UIEntityInstanceBatch) == 6976);

#pragma region Render commands

struct RENDER_COMMAND(SetFinalPassImageView)
    : renderer::RenderCommand
{
    ImageViewRef image_view;

    RENDER_COMMAND(SetFinalPassImageView)(const ImageViewRef& image_view)
        : image_view(image_view)
    {
    }

    virtual ~RENDER_COMMAND(SetFinalPassImageView)() override = default;

    virtual RendererResult operator()() override
    {
        if (!image_view)
        {
            image_view = g_render_global_state->PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView();
        }

        g_engine->GetFinalPass()->SetUILayerImageView(image_view);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RebuildProxyGroups_UI)
    : renderer::RenderCommand
{
    RC<RenderProxyList> render_proxy_list;
    Array<RenderProxy> added_proxies;
    Array<ID<Entity>> removed_entities;

    Array<Pair<ID<Entity>, int>> proxy_depths;

    FramebufferRef framebuffer;

    Optional<RenderableAttributeSet> override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<RenderProxyList>& render_proxy_list,
        Array<RenderProxy*>&& added_proxy_ptrs,
        Array<ID<Entity>>&& removed_entities,
        const Array<Pair<ID<Entity>, int>>& proxy_depths,
        const FramebufferRef& framebuffer,
        const Optional<RenderableAttributeSet>& override_attributes = {})
        : render_proxy_list(render_proxy_list),
          removed_entities(std::move(removed_entities)),
          proxy_depths(proxy_depths),
          framebuffer(framebuffer),
          override_attributes(override_attributes)
    {
        added_proxies.Reserve(added_proxy_ptrs.Size());

        for (RenderProxy* proxy_ptr : added_proxy_ptrs)
        {
            added_proxies.PushBack(*proxy_ptr);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override = default;

    RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet& entity_attributes) const
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

        RenderableAttributeSet attributes = entity_attributes;

        if (override_attributes.HasValue())
        {
            if (const ShaderDefinition& override_shader_definition = override_attributes->GetShaderDefinition())
            {
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

            if (mesh_vertex_attributes != shader_definition.GetProperties().GetRequiredVertexAttributes())
            {
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

        render_proxy_list->ClearProxyGroups();

        RenderProxyTracker& render_proxy_tracker = render_proxy_list->render_proxy_tracker;

        for (const Pair<ID<Entity>, int>& pair : proxy_depths)
        {
            RenderProxy* proxy = render_proxy_tracker.GetElement(pair.first);

            if (!proxy)
            {
                continue;
            }

            const Handle<Mesh>& mesh = proxy->mesh;
            const Handle<Material>& material = proxy->material;

            if (!mesh.IsValid() || !material.IsValid())
            {
                continue;
            }

            RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet {
                mesh->GetMeshAttributes(),
                material->GetRenderAttributes() });

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            attributes.SetDrawableLayer(pair.second);

            DrawCallCollectionMapping& mapping = render_proxy_list->mappings_by_bucket[uint32(bucket)][attributes];
            Handle<RenderGroup>& rg = mapping.render_group;

            if (!rg.IsValid())
            {
                ShaderDefinition shader_definition = attributes.GetShaderDefinition();
                shader_definition.GetProperties().Set("INSTANCING");

                ShaderRef shader = g_shader_manager->GetOrCreate(shader_definition);
                AssertThrow(shader.IsValid());

                // Create RenderGroup
                // @NOTE: Parallel disabled for now as we don't have ParallelRenderingState for UI yet.
                rg = CreateObject<RenderGroup>(
                    shader,
                    attributes,
                    RenderGroupFlags::DEFAULT & ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::PARALLEL_RENDERING));

                rg->SetDrawCallCollectionImpl(GetOrCreateDrawCallCollectionImpl<UIEntityInstanceBatch>());

                rg->AddFramebuffer(framebuffer);

#ifdef HYP_DEBUG_MODE
                if (!rg.IsValid())
                {
                    HYP_LOG(UI, Error, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                    continue;
                }
#endif

                InitObject(rg);

                if (rg->GetFlags() & RenderGroupFlags::INDIRECT_RENDERING)
                {
                    AssertDebugMsg(mapping.indirect_renderer == nullptr, "Indirect renderer already exists on mapping");

                    mapping.indirect_renderer = new IndirectRenderer();
                    mapping.indirect_renderer->Create(rg->GetDrawCallCollectionImpl());

                    mapping.draw_call_collection.impl = rg->GetDrawCallCollectionImpl();
                }
            }

            rg->AddRenderProxy(proxy);
        }

        render_proxy_list->RemoveEmptyProxyGroups();
    }

    bool RemoveRenderProxy(RenderProxyTracker& render_proxy_tracker, ID<Entity> entity)
    {
        HYP_SCOPE;

        bool removed = false;

        for (auto& mappings : render_proxy_list->mappings_by_bucket)
        {
            for (auto& it : mappings)
            {
                const DrawCallCollectionMapping& mapping = it.second;
                AssertDebug(mapping.IsValid());

                removed |= mapping.render_group->RemoveRenderProxy(entity);
            }
        }

        render_proxy_tracker.MarkToRemove(entity);

        return removed;
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups");

        RenderProxyTracker& render_proxy_tracker = render_proxy_list->render_proxy_tracker;

        // Reserve to prevent iterator invalidation
        render_proxy_tracker.Reserve(added_proxies.Size());

        // Claim before unclaiming items from removed_entities so modified proxies (which would be in removed_entities)
        // don't have their resources destroyed unnecessarily, causing destroy + recreate to occur much too frequently.
        for (RenderProxy& proxy : added_proxies)
        {
            proxy.IncRefs();
        }

        for (ID<Entity> entity : removed_entities)
        {
            const RenderProxy* proxy = render_proxy_tracker.GetElement(entity);
            AssertThrow(proxy != nullptr);

            proxy->DecRefs();

            render_proxy_tracker.MarkToRemove(entity);
        }

        for (RenderProxy& proxy : added_proxies)
        {
            render_proxy_tracker.Track(proxy.entity.GetID(), std::move(proxy));
        }

        render_proxy_tracker.Advance(AdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RenderUI)
    : renderer::RenderCommand
{
    RenderWorld* world;
    RenderView* view;
    FramebufferRef framebuffer;
    UIRenderCollector& render_collector;

    RENDER_COMMAND(RenderUI)(RenderWorld* world, RenderView* view, const FramebufferRef& framebuffer, UIRenderCollector& render_collector)
        : world(world),
          view(view),
          framebuffer(framebuffer),
          render_collector(render_collector)
    {
        AssertThrow(world != nullptr);
        AssertThrow(view != nullptr);

        // world->IncRef();
        // view->IncRef();
    }

    virtual ~RENDER_COMMAND(RenderUI)() override
    {
        world->DecRef();
        view->DecRef();
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Render UI");

        FrameBase* frame = g_rendering_api->GetCurrentFrame();

        RenderSetup render_setup { world, view };

        render_collector.CollectDrawCalls(frame, render_setup);
        render_collector.ExecuteDrawCalls(frame, render_setup, framebuffer);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

UIRenderCollector::UIRenderCollector()
    : RenderCollector(),
      m_draw_collection(MakeRefCountedPtr<RenderProxyList>()) // temp
{
}

UIRenderCollector::~UIRenderCollector() = default;

void UIRenderCollector::ResetOrdering()
{
    m_proxy_depths.Clear();
}

void UIRenderCollector::PushRenderProxy(RenderProxyTracker& render_proxy_tracker, const RenderProxy& render_proxy, int computed_depth)
{
    AssertThrow(render_proxy.entity.IsValid());
    AssertThrow(render_proxy.mesh.IsValid());
    AssertThrow(render_proxy.material.IsValid());

    render_proxy_tracker.Track(render_proxy.entity, render_proxy);

    m_proxy_depths.EmplaceBack(render_proxy.entity.GetID(), computed_depth);
}

typename RenderProxyTracker::Diff UIRenderCollector::PushUpdatesToRenderThread(RenderProxyTracker& render_proxy_tracker, const FramebufferRef& framebuffer, const Optional<RenderableAttributeSet>& override_attributes)
{
    HYP_SCOPE;

    // UISubsystem can have Update() called on a task thread.
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    typename RenderProxyTracker::Diff diff = render_proxy_tracker.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<ID<Entity>> removed_entities;
        render_proxy_tracker.GetRemoved(removed_entities, true /* include_changed */);

        Array<RenderProxy*> added_proxies_ptrs;
        render_proxy_tracker.GetAdded(added_proxies_ptrs, true /* include_changed */);

        if (added_proxies_ptrs.Any() || removed_entities.Any())
        {
            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                m_draw_collection,
                std::move(added_proxies_ptrs),
                std::move(removed_entities),
                m_proxy_depths,
                framebuffer,
                override_attributes);
        }
    }

    render_proxy_tracker.Advance(AdvanceAction::CLEAR);

    return diff;
}

void UIRenderCollector::CollectDrawCalls(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::Iterator;

    Array<IteratorType> iterators;

    for (auto& mappings : m_draw_collection->mappings_by_bucket)
    {
        for (auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    TaskSystem::GetInstance().ParallelForEach(
        TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER),
        iterators,
        [](IteratorType it, uint32, uint32)
        {
            DrawCallCollectionMapping& mapping = it->second;
            AssertDebug(mapping.IsValid());

            const Handle<RenderGroup>& render_group = mapping.render_group;
            AssertDebug(render_group.IsValid());

            render_group->CollectDrawCalls(mapping.draw_call_collection);
        });
}

void UIRenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, const FramebufferRef& framebuffer) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_draw_collection != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto& mappings : m_draw_collection->mappings_by_bucket)
    {
        for (const auto& it : mappings)
        {
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

    for (SizeType index = 0; index < iterators.Size(); index++)
    {
        const auto& it = *iterators[index];

        const RenderableAttributeSet& attributes = it.first;
        const DrawCallCollectionMapping& mapping = it.second;
        AssertThrow(mapping.IsValid());

        const Handle<RenderGroup>& render_group = mapping.render_group;
        AssertThrow(render_group.IsValid());

        const DrawCallCollection& draw_call_collection = mapping.draw_call_collection;

        // Don't count draw calls for UI
        SuppressEngineRenderStatsScope suppress_render_stats_scope;

        render_group->PerformRendering(frame, render_setup, draw_call_collection, nullptr, nullptr);
    }

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frame_index);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIRenderSubsystem

UIRenderSubsystem::UIRenderSubsystem(const Handle<UIStage>& ui_stage)
    : m_ui_stage(ui_stage)
{
}

UIRenderSubsystem::~UIRenderSubsystem()
{
    HYP_SYNC_RENDER();
}

void UIRenderSubsystem::Init()
{
    HYP_SCOPE;

    struct RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)
        : renderer::RenderCommand
    {
        UIRenderSubsystem* ui_render_subsystem;

        RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)(UIRenderSubsystem* ui_render_subsystem)
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

    m_on_gbuffer_resolution_changed_handle = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([weak_this = WeakHandleFromThis()]()
        {
            Threads::AssertOnThread(g_render_thread);

            HYP_LOG(UI, Debug, "UIRenderSubsystem: resizing to {}", g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

            Handle<UIRenderSubsystem> subsystem = weak_this.Lock().Cast<UIRenderSubsystem>();

            if (!subsystem)
            {
                HYP_LOG(UI, Warning, "UIRenderSubsystem: subsystem is expired on resize");

                return;
            }

            PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

            SafeRelease(std::move(subsystem->m_framebuffer));

            PUSH_RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer, subsystem);
        });

    AssertThrow(m_ui_stage != nullptr);
    AssertThrow(m_ui_stage->GetCamera().IsValid());
    AssertThrow(m_ui_stage->GetCamera()->IsReady());

    m_camera_resource_handle = TResourceHandle<RenderCamera>(m_ui_stage->GetCamera()->GetRenderResource());

    m_view = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::DEFAULT & ~ViewFlags::ALL_WORLD_SCENES,
        .viewport = Viewport { .extent = Vec2u(m_ui_stage->GetSurfaceSize()), .position = Vec2i::Zero() },
        .scenes = { m_ui_stage->GetScene()->HandleFromThis() },
        .camera = m_ui_stage->GetCamera() });
    InitObject(m_view);

    PUSH_RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer, this);
}

void UIRenderSubsystem::OnAddedToWorld()
{
    HYP_SCOPE;
}

void UIRenderSubsystem::OnRemovedFromWorld()
{
    m_view.Reset();

    PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

    SafeRelease(std::move(m_framebuffer));

    m_on_gbuffer_resolution_changed_handle.Reset();
}

void UIRenderSubsystem::Update(float delta)
{
    g_engine->GetWorld()->GetRenderResource().IncRef();
    m_view->GetRenderResource().IncRef();

    PUSH_RENDER_COMMAND(
        RenderUI,
        &g_engine->GetWorld()->GetRenderResource(),
        &m_view->GetRenderResource(),
        m_framebuffer,
        m_render_collector);
}

void UIRenderSubsystem::CreateFramebuffer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const Vec2u surface_size = Vec2u(g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

    m_view->SetViewport(Viewport {
        .extent = surface_size,
        .position = Vec2i::Zero() });

    m_framebuffer = g_rendering_api->MakeFramebuffer(Vec2u(surface_size) * 2);

    AttachmentRef color_attachment = m_framebuffer->AddAttachment(
        0,
        InternalFormat::RGBA16F,
        ImageType::TEXTURE_TYPE_2D,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    m_framebuffer->AddAttachment(
        1,
        g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
        ImageType::TEXTURE_TYPE_2D,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    DeferCreate(m_framebuffer);

    m_camera_resource_handle->SetFramebuffer(m_framebuffer);

    const ThreadID owner_thread_id = m_ui_stage->GetScene()->GetEntityManager()->GetOwnerThreadID();

    if (Threads::IsOnThread(owner_thread_id))
    {
        m_ui_stage->SetSurfaceSize(Vec2i(surface_size));
    }
    else
    {
        Threads::GetThread(owner_thread_id)->GetScheduler().Enqueue([ui_stage = m_ui_stage, surface_size]()
            {
                ui_stage->SetSurfaceSize(Vec2i(surface_size));
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    PUSH_RENDER_COMMAND(SetFinalPassImageView, color_attachment->GetImageView());
}

#pragma endregion UIRenderSubsystem

} // namespace hyperion
