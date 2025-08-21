/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/Scene.hpp>
#include <scene/View.hpp>

#include <scene/EntityManager.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <rendering/UIRenderer.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

struct alignas(16) UIEntityInstanceBatch : EntityInstanceBatch
{
    Vec4f texcoords[maxEntitiesPerInstanceBatch];
    Vec4f offsets[maxEntitiesPerInstanceBatch];
    Vec4f sizes[maxEntitiesPerInstanceBatch];
    Vec4u properties[maxEntitiesPerInstanceBatch];
};

#pragma region Render commands

struct RENDER_COMMAND(AddUIRenderer)
    : RenderCommand
{
    UIRenderer* uiRenderer;

    RENDER_COMMAND(AddUIRenderer)(UIRenderer* uiRenderer)
        : uiRenderer(uiRenderer)
    {
        AssertDebug(uiRenderer != nullptr);
    }

    virtual RendererResult operator()() override
    {
        g_renderGlobalState->AddRenderer(GRT_UI, uiRenderer);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveUIRenderer)
    : RenderCommand
{
    UIRenderer* uiRenderer;

    RENDER_COMMAND(RemoveUIRenderer)(UIRenderer* uiRenderer)
        : uiRenderer(uiRenderer)
    {
    }

    virtual RendererResult operator()() override
    {
        g_renderGlobalState->RemoveRenderer(GRT_UI, uiRenderer);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(SetFinalPassImageView)
    : RenderCommand
{
    GpuImageViewRef imageView;

    RENDER_COMMAND(SetFinalPassImageView)(const GpuImageViewRef& imageView)
        : imageView(imageView)
    {
    }

    virtual ~RENDER_COMMAND(SetFinalPassImageView)() override = default;

    virtual RendererResult operator()() override
    {
        if (!imageView)
        {
            imageView = g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d);
        }

        g_engineDriver->GetFinalPass()->SetUILayerImageView(imageView);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

UISubsystem::UISubsystem(const Handle<UIStage>& uiStage)
    : m_uiStage(uiStage),
      m_uiRenderer(nullptr)
{
}

UISubsystem::~UISubsystem()
{
    if (m_uiRenderer)
    {
        PUSH_RENDER_COMMAND(RemoveUIRenderer, m_uiRenderer);
        m_uiRenderer = nullptr;
    }
}

void UISubsystem::Init()
{
    HYP_SCOPE;

    Assert(m_uiStage != nullptr);
    InitObject(m_uiStage);

    m_onResizeHandle = g_appContext->GetMainWindow()->OnWindowSizeChanged.BindThreaded([weakThis = WeakHandleFromThis()](Vec2i windowSize)
        {
            PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

            Handle<UISubsystem> subsystem = weakThis.Lock();

            if (!subsystem)
            {
                HYP_LOG(UI, Warning, "UISubsystem: subsystem is expired on resize");

                return;
            }

            Handle<UIStage> uiStage = subsystem->GetUIStage();
            AssertDebug(uiStage != nullptr);

            uiStage->SetSurfaceSize(windowSize);

            subsystem->CreateFramebuffer();
        },
        g_gameThread);

    const Vec2u surfaceSize = Vec2u(m_uiStage->GetSurfaceSize());
    HYP_LOG(UI, Debug, "UISubsystem: surface size is {}", surfaceSize);

    ViewOutputTargetDesc outputTargetDesc {
        .extent = surfaceSize * 2,
        .attachments = { { TF_RGBA16F }, { g_renderBackend->GetDefaultFormat(DIF_DEPTH) } }
    };

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT & ~ViewFlags::ALL_WORLD_SCENES,
        .viewport = Viewport { .extent = surfaceSize, .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = { m_uiStage->GetScene() },
        .camera = m_uiStage->GetCamera(),
        .drawCallCollectionImpl = GetOrCreateDrawCallCollectionImpl<UIEntityInstanceBatch>()
    };

    m_view = CreateObject<View>(viewDesc);
    InitObject(m_view);

    CreateFramebuffer();

    m_uiRenderer = new UIRenderer(m_view);
    m_uiRenderer->renderCollector.drawCallCollectionImpl = viewDesc.drawCallCollectionImpl;

    PUSH_RENDER_COMMAND(AddUIRenderer, m_uiRenderer);
}

void UISubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    if (m_uiStage && m_uiStage->GetScene())
    {
        GetWorld()->AddScene(MakeStrongRef(m_uiStage->GetScene()));
    }
}

void UISubsystem::OnRemovedFromWorld()
{
    if (m_uiStage && m_uiStage->GetScene())
    {
        GetWorld()->RemoveScene(m_uiStage->GetScene());
    }

    PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

    m_onResizeHandle.Reset();
}

void UISubsystem::PreUpdate(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
}

void UISubsystem::Update(float delta)
{
    m_uiStage->Update(delta);

    m_view->UpdateViewport();
    m_view->UpdateVisibility();

    RenderProxyList& rpl = RenderApi_GetProducerProxyList(m_view);
    rpl.BeginWrite();
    rpl.disableBuildRenderCollection = true;
    rpl.useOrdering = true;
    rpl.viewport = m_view->GetViewport();
    rpl.priority = m_view->GetPriority();

    rpl.meshEntityOrdering.Clear();

    rpl.GetCameras().Track(m_view->GetCamera()->Id(), m_view->GetCamera(), m_view->GetCamera()->GetRenderProxyVersionPtr());

    m_uiStage->CollectObjects([&rpl](UIObject* uiObject)
        {
            Assert(uiObject != nullptr);

            const Handle<Entity>& entity = uiObject->GetEntity();
            Assert(entity != nullptr);

            MeshComponent& meshComponent = entity->GetComponent<MeshComponent>();

            // @TODO Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            rpl.GetMeshEntities().Track(entity.Id(), entity, entity->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ false);

            if (const Handle<Material>& material = meshComponent.material)
            {
                rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);

                for (const auto& it : material->GetTextures())
                {
                    const Handle<Texture>& texture = it.second;

                    if (!texture.IsValid())
                    {
                        continue;
                    }

                    rpl.GetTextures().Track(texture.Id(), texture.Get());
                }
            }

            rpl.meshEntityOrdering.EmplaceBack(entity.Id(), uiObject->GetComputedDepth());
        },
        /* onlyVisible */ true);

    ResourceTrackerDiff meshesDiff = rpl.GetMeshEntities().GetDiff();

    if (meshesDiff.NeedsUpdate())
    {
        Array<Entity*> added;
        rpl.GetMeshEntities().GetAdded(added, /* includeChanged */ true);

        for (Entity* entity : added)
        {
            AssertDebug(entity->InstanceClass() == Entity::Class());
            
            auto&& [meshComponent, transformComponent, boundingBoxComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent>(entity);
            AssertDebug(meshComponent != nullptr);

            RenderProxyMesh& meshProxy = *rpl.GetMeshEntities().SetProxy(entity->Id(), RenderProxyMesh());
            meshProxy.entity = MakeWeakRef(entity);
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.instanceData = meshComponent->instanceData;
            meshProxy.bufferData.modelMatrix = transformComponent ? transformComponent->transform.GetMatrix() : Matrix4::Identity();
            meshProxy.bufferData.previousModelMatrix = meshComponent->previousModelMatrix;
            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
            meshProxy.bufferData.userData = reinterpret_cast<EntityShaderData::EntityUserData&>(meshComponent->userData);
        }
    }

    rpl.EndWrite();
}

void UISubsystem::CreateFramebuffer()
{
    HYP_SCOPE;

    const ThreadId ownerThreadId = m_uiStage->GetScene()->GetEntityManager()->GetOwnerThreadId();

    auto impl = [weakThis = WeakHandleFromThis()]()
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view");

        Handle<UISubsystem> subsystem = weakThis.Lock();

        if (!subsystem)
        {
            HYP_LOG(UI, Warning, "UISubsystem: subsystem is expired while creating view");

            return;
        }
    };

    if (Threads::IsOnThread(ownerThreadId))
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view on owner thread");

        impl();
    }
    else
    {
        Threads::GetThread(ownerThreadId)->GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    const ViewOutputTarget& outputTarget = m_view->GetOutputTarget();
    Assert(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    Assert(framebuffer.IsValid());

    const AttachmentBase* attachment = framebuffer->GetAttachment(0);
    Assert(attachment != nullptr);

    Assert(attachment->GetImageView().IsValid());
    // Assert(attachment->GetImageView()->IsCreated());

    PUSH_RENDER_COMMAND(SetFinalPassImageView, attachment->GetImageView());
}

} // namespace hyperion
