/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <UIPch.hpp>

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
#include <scene/LightmapVolume.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <rendering/FinalPass.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/InstancedMeshData.hpp>

#include <rendering/renderers/UIRenderer.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

#include <UISubsystem.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

HYP_REGISTER_DRAW_BATCH_TYPE(UIEntityInstanceBatch);

#pragma region Render commands

struct AddUIRendererForView : RenderCommand
{
    WeakHandle<View> viewWeak;

    AddUIRendererForView(const WeakHandle<View>& viewWeak)
        : viewWeak(viewWeak)
    {
    }

    virtual RendererResult operator()() override
    {
        Handle<View> view = viewWeak.Lock();
        if (!view)
        {
            HYP_LOG(UI, Warning, "AddUIRendererForView: view is expired");

            return {};
        }

        UIRenderer* uiRenderer = PoolNew<UIRenderer>(*g_renderPool, view);

        const Class* entityBatchClass = view->GetViewDesc().entityBatchClass;

        if (entityBatchClass != nullptr)
        {
            uiRenderer->renderCollector.batchAllocator = GetOrCreateEntityBatchAllocator(entityBatchClass->GetTypeId());
        }

        g_renderInterface->AddRenderer(GRT_UI, uiRenderer);

        return {};
    }
};

struct RemoveUIRenderer : RenderCommand
{
    UIRenderer* uiRenderer;

    RemoveUIRenderer(UIRenderer* uiRenderer)
        : uiRenderer(uiRenderer)
    {
    }

    virtual RendererResult operator()() override
    {
        g_renderInterface->RemoveRenderer(GRT_UI, uiRenderer);

        return {};
    }
};

struct SetFinalPassImageView : RenderCommand
{
    GpuImageViewRef imageView;

    SetFinalPassImageView(const GpuImageViewRef& imageView)
        : imageView(imageView)
    {
    }

    virtual ~SetFinalPassImageView() override = default;

    virtual RendererResult operator()() override
    {
        if (!imageView)
        {
            imageView = g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->placeholderData->defaultTexture2d);
        }

        g_renderInterface->finalPass->SetUILayerImageView(imageView);

        return {};
    }
};

#pragma endregion Render commands

UISubsystem::UISubsystem()
    : UISubsystem(MakeHandle<UIStage>())
{
}

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

    PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

    m_onWindowResizedHandle.Reset();
    m_onCurrentWindowChangedHandle.Reset();
}

void UISubsystem::Init()
{
    Subsystem::Init();

    Assert(m_uiStage != nullptr);
    InitObject(m_uiStage);

    const auto handleWindowResize = [weakThis = MakeWeakRef(this)](Vec2i windowSize)
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
    };

    Vec2u windowSize = Vec2u(800, 600);

    if (g_appContext->GetMainWindow() != nullptr)
    {
        windowSize = Vec2u(g_appContext->GetMainWindow()->GetSize());

        m_onWindowResizedHandle = g_appContext->GetMainWindow()->OnWindowSizeChanged.BindThreaded(handleWindowResize, g_simThread);
    }

    m_onCurrentWindowChangedHandle = g_appContext->OnCurrentWindowChanged.BindThreaded(
        [weakThis = MakeWeakRef(this), handleWindowResize](ApplicationWindow* window)
        {
            Handle<UISubsystem> subsystem = weakThis.Lock();

            if (!subsystem)
            {
                HYP_LOG(UI, Warning, "UISubsystem: subsystem is expired on current window changed");
                return;
            }
            if (subsystem->m_onWindowResizedHandle.IsValid())
            {
                subsystem->m_onWindowResizedHandle.Reset();
            }

            if (window != nullptr)
            {
                subsystem->m_onWindowResizedHandle = window->OnWindowSizeChanged.BindThreaded(handleWindowResize, g_simThread);

                handleWindowResize(Vec2i(window->GetSize()));
            }
        },
        g_simThread);

    const Vec2u windowSize2 = windowSize * 2;

    RenderTargetDesc renderTargetDesc;
    renderTargetDesc.extent = windowSize2;
    renderTargetDesc.AddAttachment({ TextureType::Texture2D, TextureFormat::RGBA8 });

    ViewDesc viewDesc {};
    viewDesc.flags = (ViewFlags::DEFAULT & ~(ViewFlags::ALL_WORLD_SCENES | ViewFlags::MATCH_CAMERA_DIMENSIONS));
    viewDesc.renderTargetDesc = renderTargetDesc;
    viewDesc.scenes = { m_uiStage->GetScene() };
    viewDesc.camera = m_uiStage->GetCamera();
    viewDesc.entityBatchClass = UIEntityInstanceBatch::StaticClass();

    m_view = MakeHandle<View>(viewDesc);
    m_view->SetName(NAME("UISubsystem_View"));
    InitObject(m_view);

    CreateFramebuffer();

    PUSH_RENDER_COMMAND(AddUIRendererForView, m_view);
}

void UISubsystem::OnAddedToWorld()
{
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
}

void UISubsystem::PreUpdate(float delta)
{
}

void UISubsystem::Update(float delta)
{
    HYP_SCOPE;

    m_uiStage->Update(delta);

    m_view->SetOverrideCollectFunctor(ProcRef<void(RenderProxyList&)>(*this, ValueWrapper<&UISubsystem::RenderCollect>()));

    GetWorld()->ProcessViewAsync(m_view);
}

void UISubsystem::RenderCollect(RenderProxyList& rpl)
{
    rpl.disableBuildRenderCollection = true;
    rpl.useOrdering = true;
    rpl.priority = m_view->GetPriority();

    rpl.meshEntityOrdering.Clear();

    rpl.GetCameras().Track(m_view->GetCamera()->Id(), m_view->GetCamera(), m_view->GetCamera()->GetRenderProxyVersionPtr());

    m_uiStage->CollectObjects([&rpl](UIObject* uiObject)
        {
            AssertDebug(uiObject != nullptr);

            const Handle<Entity>& entity = uiObject->GetEntity();
            AssertDebug(entity != nullptr);

            MeshComponent& meshComponent = entity->GetComponent<MeshComponent>();

            /// \todo Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            rpl.GetMeshEntities().Track(entity.Id(), entity, entity->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ false);

            if (Mesh* mesh = meshComponent.mesh)
            {
                rpl.GetMeshes().Track(mesh->Id(), mesh);
            }

            if (Material* material = meshComponent.material)
            {
                rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);

                for (Texture* texture : material->GetTextures())
                {
                    if (!texture)
                    {
                        continue;
                    }

                    rpl.GetTextures().Track(texture->Id(), texture);
                }
            }

            rpl.meshEntityOrdering.EmplaceBack(entity.Id(), uiObject->GetComputedDepth());
        },
        /* onlyVisible */ true);

    ResourceTrackerDiff meshesDiff = rpl.GetMeshEntities().GetDiff();

    if (meshesDiff.NeedsUpdate())
    {
        Array<Entity*> entities;
        rpl.GetMeshEntities().GetAdded(entities, /* includeChanged */ true);

        for (Entity* entity : entities)
        {
            AssertDebug(entity->InstanceClass() == Entity::StaticClass());

            auto&& [meshComponent, transformComponent, boundingBoxComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent>(entity);
            AssertDebug(meshComponent != nullptr);

            RenderProxyMesh& meshProxy = *rpl.GetMeshEntities().SetProxy(entity->Id(), RenderProxyMesh());

            if ((meshComponent->enableAutoInstancing || meshComponent->numInstances > 1)
                && meshComponent->instanceData.IsLoaded())
            {
                const Handle<InstancedMeshData>& instancedMesh = ObjCast<InstancedMeshData>(meshComponent->instanceData.Resolve());
                Assert(instancedMesh.IsValid());

                for (uint32 i = 0; i < uint32(instancedMesh->buffers.Size()); i++)
                {
                    if (instancedMesh->buffers[i].size == 0)
                        continue;

                    meshProxy.instanceData.buffers[i].SetSize(instancedMesh->buffers[i].size, false);

                    AssertDebug(instancedMesh->buffers[i].raw != nullptr);
                    Memory::Copy(meshProxy.instanceData.buffers[i].Data(), instancedMesh->buffers[i].raw, instancedMesh->buffers[i].size);

                    meshProxy.instanceData.bufferStructSizes[i] = instancedMesh->bufferStructSizes[i];
                    meshProxy.instanceData.bufferStructAlignments[i] = instancedMesh->bufferStructAlignments[i];
                }
            }
            else
            {
                meshProxy.instanceData = {};
            }

            meshProxy.version = *entity->GetRenderProxyVersionPtr();
            meshProxy.forceRebind = false;
            meshProxy.entity = MakeWeakRef(entity);
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.numIndices = meshComponent->mesh->NumIndices();
            meshProxy.numInstances = meshComponent->numInstances;
            meshProxy.enableAutoInstancing = meshComponent->enableAutoInstancing;
            meshProxy.cachedAttributes = RenderableAttributeSet(meshComponent->mesh->GetMeshAttributes(), meshComponent->material->GetRenderAttributes());
            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
        }
    }
}

void UISubsystem::CreateFramebuffer()
{
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

    if (IsOnThread(ownerThreadId))
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view on owner thread");

        impl();
    }
    else
    {
        GetThreadById(ownerThreadId)->GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
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

} // namespace Hyperion
