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

#pragma region UIPassData

struct UIPassData : PassData
{
};

#pragma endregion UIPassData

#pragma region Render commands

struct RENDER_COMMAND(SetFinalPassImageView)
    : RenderCommand
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
    : RenderCommand
{
    RC<RenderProxyList> render_proxy_list;
    Array<RenderProxy> added_proxies;
    Array<ID<Entity>> removed_entities;

    Array<Pair<ID<Entity>, int>> proxy_depths;

    Optional<RenderableAttributeSet> override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<RenderProxyList>& render_proxy_list,
        Array<RenderProxy*>&& added_proxy_ptrs,
        Array<ID<Entity>>&& removed_entities,
        const Array<Pair<ID<Entity>, int>>& proxy_depths,
        const Optional<RenderableAttributeSet>& override_attributes = {})
        : render_proxy_list(render_proxy_list),
          removed_entities(std::move(removed_entities)),
          proxy_depths(proxy_depths),
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

        render_proxy_list->Clear();

        ResourceTracker<ID<Entity>, RenderProxy>& meshes = render_proxy_list->meshes;

        for (const Pair<ID<Entity>, int>& pair : proxy_depths)
        {
            RenderProxy* proxy = meshes.GetElement(pair.first);

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

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            attributes.SetDrawableLayer(pair.second);

            DrawCallCollectionMapping& mapping = render_proxy_list->mappings_by_bucket[rb][attributes];
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

            auto insert_result = mapping.render_proxies.Insert(proxy->entity.GetID(), proxy);
            AssertDebug(insert_result.second);
        }

        render_proxy_list->RemoveEmptyRenderGroups();
    }

    bool RemoveRenderProxy(ResourceTracker<ID<Entity>, RenderProxy>& meshes, ID<Entity> entity)
    {
        HYP_SCOPE;

        bool removed = false;

        for (auto& mappings : render_proxy_list->mappings_by_bucket)
        {
            for (auto& it : mappings)
            {
                DrawCallCollectionMapping& mapping = it.second;
                AssertDebug(mapping.IsValid());

                auto proxy_iter = mapping.render_proxies.Find(entity);

                if (proxy_iter != mapping.render_proxies.End())
                {
                    mapping.render_proxies.Erase(proxy_iter);

                    removed = true;

                    continue;
                }
            }
        }

        meshes.MarkToRemove(entity);

        return removed;
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups");

        ResourceTracker<ID<Entity>, RenderProxy>& meshes = render_proxy_list->meshes;

        // Claim before unclaiming items from removed_entities so modified proxies (which would be in removed_entities)
        // don't have their resources destroyed unnecessarily, causing destroy + recreate to occur much too frequently.
        for (RenderProxy& proxy : added_proxies)
        {
            proxy.IncRefs();
        }

        for (ID<Entity> entity : removed_entities)
        {
            const RenderProxy* proxy = meshes.GetElement(entity);
            AssertThrow(proxy != nullptr);

            proxy->DecRefs();

            meshes.MarkToRemove(entity);
        }

        for (RenderProxy& proxy : added_proxies)
        {
            meshes.Track(proxy.entity.GetID(), std::move(proxy));
        }

        meshes.Advance(AdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RenderUI)
    : RenderCommand
{
    UIRenderer* ui_renderer;
    Handle<UIStage> ui_stage;
    RenderWorld* world;
    RenderView* view;

    RENDER_COMMAND(RenderUI)(UIRenderer* ui_renderer, const Handle<UIStage>& ui_stage, RenderWorld* world, RenderView* view)
        : ui_renderer(ui_renderer),
          ui_stage(ui_stage),
          world(world),
          view(view)
    {
        AssertThrow(ui_renderer != nullptr);
        AssertThrow(ui_stage.IsValid());
        AssertThrow(world != nullptr);
        AssertThrow(view != nullptr);
    }

    virtual ~RENDER_COMMAND(RenderUI)() override
    {
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Render UI");

        FrameBase* frame = g_rendering_api->GetCurrentFrame();

        RenderSetup render_setup { world, view };

        ui_renderer->RenderFrame(frame, render_setup);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

UIRenderCollector::UIRenderCollector()
    : RenderCollector(),
      rpl(MakeRefCountedPtr<RenderProxyList>())
{
}

UIRenderCollector::~UIRenderCollector() = default;

void UIRenderCollector::ResetOrdering()
{
    proxy_depths.Clear();
}

void UIRenderCollector::PushRenderProxy(ResourceTracker<ID<Entity>, RenderProxy>& meshes, const RenderProxy& render_proxy, int computed_depth)
{
    AssertThrow(render_proxy.entity.IsValid());
    AssertThrow(render_proxy.mesh.IsValid());
    AssertThrow(render_proxy.material.IsValid());

    meshes.Track(render_proxy.entity, render_proxy);

    proxy_depths.EmplaceBack(render_proxy.entity.GetID(), computed_depth);
}

typename ResourceTracker<ID<Entity>, RenderProxy>::Diff UIRenderCollector::PushUpdatesToRenderThread(ResourceTracker<ID<Entity>, RenderProxy>& meshes, const Optional<RenderableAttributeSet>& override_attributes)
{
    HYP_SCOPE;

    // UISubsystem can have Update() called on a task thread.
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    auto diff = meshes.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<ID<Entity>> removed_entities;
        meshes.GetRemoved(removed_entities, true /* include_changed */);

        Array<RenderProxy*> added_proxies_ptrs;
        meshes.GetAdded(added_proxies_ptrs, true /* include_changed */);

        if (added_proxies_ptrs.Any() || removed_entities.Any())
        {
            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                rpl,
                std::move(added_proxies_ptrs),
                std::move(removed_entities),
                proxy_depths,
                override_attributes);
        }
    }

    meshes.Advance(AdvanceAction::CLEAR);

    return diff;
}

void UIRenderCollector::CollectDrawCalls(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    RenderCollector::CollectDrawCalls(*rpl, 0);
}

void UIRenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, const FramebufferRef& framebuffer) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(rpl != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto& mappings : rpl->mappings_by_bucket)
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

#pragma region UIRenderer

void UIRenderer::Initialize()
{
}

void UIRenderer::Shutdown()
{
}

void UIRenderer::RenderFrame(FrameBase* frame, const RenderSetup& render_setup)
{
    View* view = render_setup.view->GetView();
    AssertThrow(view != nullptr);

    UIPassData*& pd = m_view_pass_data[view];

    if (!pd)
    {
        pd = &m_pass_data.EmplaceBack();

        CreateViewPassData(view, *pd);

        pd->viewport = view->GetRenderResource().GetViewport(); // rpl.viewport;
    }
    else if (pd->viewport != view->GetRenderResource().GetViewport())
    {
        /// @TODO: Implement me!

        HYP_LOG(UI, Warning, "UIRenderer: Viewport size changed from {} to {}, resizing view pass data",
            pd->viewport.extent, view->GetRenderResource().GetViewport().extent);

        // ResizeView(view->GetRenderResource().GetViewport(), view, *pd);
    }

    RenderSetup rs = render_setup;
    rs.pass_data = pd;

    const ViewOutputTarget& output_target = view->GetOutputTarget();
    AssertThrow(output_target.IsValid());

    m_render_collector.CollectDrawCalls(frame, rs);
    m_render_collector.ExecuteDrawCalls(frame, rs, output_target.GetFramebuffer());
}

void UIRenderer::CreateViewPassData(View* view, PassData& pass_data)
{
    UIPassData& pd = static_cast<UIPassData&>(pass_data);

    pd.view = view->WeakHandleFromThis();
    pd.viewport = view->GetRenderResource().GetViewport();

    HYP_LOG(UI, Debug, "Creating UI pass data with viewport size {}", pd.viewport.extent);
}

#pragma endregion UIRenderer

#pragma region UIRenderSubsystem

UIRenderSubsystem::UIRenderSubsystem(const Handle<UIStage>& ui_stage)
    : m_ui_stage(ui_stage),
      m_ui_renderer(nullptr)
{
}

UIRenderSubsystem::~UIRenderSubsystem()
{
    // push render command to delete UIRenderer on the render thread
    if (m_ui_renderer)
    {
        struct RENDER_COMMAND(DeleteUIRenderer)
            : RenderCommand
        {
            UIRenderer* ui_renderer;

            RENDER_COMMAND(DeleteUIRenderer)(UIRenderer* ui_renderer)
                : ui_renderer(ui_renderer)
            {
            }

            virtual ~RENDER_COMMAND(DeleteUIRenderer)() override = default;

            virtual RendererResult operator()() override
            {
                delete ui_renderer;

                HYPERION_RETURN_OK;
            }
        };

        PUSH_RENDER_COMMAND(DeleteUIRenderer, m_ui_renderer);
        m_ui_renderer = nullptr;
    }
}

void UIRenderSubsystem::Init()
{
    HYP_SCOPE;

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

            subsystem->CreateFramebuffer();
        });

    AssertThrow(m_ui_stage != nullptr);
    AssertThrow(m_ui_stage->GetCamera().IsValid());
    AssertThrow(m_ui_stage->GetCamera()->IsReady());

    m_ui_renderer = new UIRenderer();

    m_camera_resource_handle = TResourceHandle<RenderCamera>(m_ui_stage->GetCamera()->GetRenderResource());

    const Vec2u surface_size = Vec2u(m_ui_stage->GetSurfaceSize());
    HYP_LOG(UI, Debug, "UIRenderSubsystem: surface size is {}", surface_size);

    ViewOutputTargetDesc output_target_desc {
        .extent = surface_size * 2,
        .attachments = { { TF_RGBA8 }, { g_rendering_api->GetDefaultFormat(DIF_DEPTH) } }
    };

    ViewDesc view_desc {
        .flags = ViewFlags::DEFAULT & ~ViewFlags::ALL_WORLD_SCENES,
        .viewport = Viewport { .extent = surface_size, .position = Vec2i::Zero() },
        .output_target_desc = output_target_desc,
        .scenes = { m_ui_stage->GetScene()->HandleFromThis() },
        .camera = m_ui_stage->GetCamera()
    };

    m_view = CreateObject<View>(view_desc);
    InitObject(m_view);

    // temp shit
    m_view->GetRenderResource().IncRef();

    CreateFramebuffer();
}

void UIRenderSubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    if (m_view)
    {
        m_view->GetRenderResource().IncRef();
    }
}

void UIRenderSubsystem::OnRemovedFromWorld()
{
    if (m_view)
    {
        m_view->GetRenderResource().DecRef();
        m_view.Reset();
    }

    PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

    m_on_gbuffer_resolution_changed_handle.Reset();
}

void UIRenderSubsystem::PreUpdate(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    m_ui_stage->Update(delta);
}

void UIRenderSubsystem::Update(float delta)
{
    UIRenderCollector& render_collector = m_ui_renderer->GetRenderCollector();
    render_collector.ResetOrdering();

    m_ui_stage->CollectObjects([&render_collector, &meshes = m_render_proxy_tracker](UIObject* ui_object)
        {
            AssertThrow(ui_object != nullptr);

            const Handle<Node>& node = ui_object->GetNode();
            AssertThrow(node.IsValid());

            const Handle<Entity>& entity = node->GetEntity();

            MeshComponent& mesh_component = node->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(entity);

            if (!mesh_component.proxy)
            {
                return;
            }

            // @TODO Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            render_collector.PushRenderProxy(meshes, *mesh_component.proxy, ui_object->GetComputedDepth());
        },
        /* only_visible */ true);

    render_collector.PushUpdatesToRenderThread(m_render_proxy_tracker);

    PUSH_RENDER_COMMAND(RenderUI, m_ui_renderer, m_ui_stage, &GetWorld()->GetRenderResource(), &m_view->GetRenderResource());
}

void UIRenderSubsystem::CreateFramebuffer()
{
    HYP_SCOPE;

    const ThreadID owner_thread_id = m_ui_stage->GetScene()->GetEntityManager()->GetOwnerThreadID();

    auto impl = [weak_this = WeakHandleFromThis()]()
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view");

        Handle<UIRenderSubsystem> subsystem = weak_this.Lock().Cast<UIRenderSubsystem>();

        if (!subsystem)
        {
            HYP_LOG(UI, Warning, "UIRenderSubsystem: subsystem is expired while creating view");

            return;
        }

        const Vec2u surface_size = Vec2u(g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

        // subsystem->m_view->SetViewport(Viewport { .extent = surface_size, .position = Vec2i::Zero() });

        // subsystem->m_ui_stage->SetSurfaceSize(Vec2i(surface_size));

        HYP_LOG(UI, Debug, "Created UI Render Subsystem view with ID {} and surface size {}",
            subsystem->m_view->GetID().Value(), surface_size);
    };

    if (Threads::IsOnThread(owner_thread_id))
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view on owner thread");

        impl();
    }
    else
    {
        Threads::GetThread(owner_thread_id)->GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    const ViewOutputTarget& output_target = m_view->GetOutputTarget();
    AssertThrow(output_target.IsValid());

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    const AttachmentBase* attachment = framebuffer->GetAttachment(0);
    AssertThrow(attachment != nullptr);

    AssertThrow(attachment->GetImageView().IsValid());
    // AssertThrow(attachment->GetImageView()->IsCreated());

    PUSH_RENDER_COMMAND(SetFinalPassImageView, attachment->GetImageView());
}

#pragma endregion UIRenderSubsystem

} // namespace hyperion
