/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIStage.hpp>
#include <UI/UIListView.hpp>

#include <UI/Font/FontAtlas.hpp>

#include <Scene/World.hpp>
#include <Scene/Node.hpp>
#include <Scene/Scene.hpp>
#include <Scene/View.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/EnvGrid.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Light.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

#include <Rendering/FinalPass.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialInstance.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/InstancedMeshData.hpp>

#include <Rendering/Passes/UIPass.hpp>

#include <System/AppContext.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

// to be moved
#include <UI/Overlays/Overlay.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

#include <UISubsystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(UI);

HYP_REGISTER_DRAW_BATCH_TYPE(UIEntityInstanceBatch);

extern CVar<bool> cvShowDebugUI;

#pragma region Render commands

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
            imageView = RI.textureViewCache->GetOrCreate(RI.placeholderData->defaultTexture2d);
        }

        RI.finalPass->SetUILayerImageView(imageView);

        return {};
    }
};

#pragma endregion Render commands

static TResult<Handle<FontAtlas>> CreateFontAtlas()
{
    // we check if it exists in the registry before creating.
    // some platforms build without freetype support so the atlas must already exist in the registry.

    AssetRegistry& registry = *GetEngineAssetRegistry();

    GlobalContextScope contextScope { AssetRegistryContext { MakeStrongRef(&registry) } };

    Handle<FontAtlas> fontAtlas = DynamicCast<FontAtlas>(registry.GetAsset(AssetBuckets::FontAtlases, "Roboto_Regular"_sh));

    if (fontAtlas.IsValid())
    {
        return fontAtlas;
    }

    auto fontFaceAsset = g_assetManager->Load<RC<FontFace>>("Fonts/Roboto/Roboto-Regular.ttf");

    if (fontFaceAsset.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load font face! Error: {}", fontFaceAsset.GetError().GetMessage());
    }

    // create new font atlas
    fontAtlas = MakeHandle<FontAtlas>(NAME("Roboto_Regular"), std::move(fontFaceAsset->Result()));

    // render atlas textures.
    if (Result renderAtlasResult = fontAtlas->RenderAtlasTextures(1.0f, 2.0f, 0.1f); renderAtlasResult.HasError())
    {
        return renderAtlasResult.GetError();
    }

    registry.PutAssetsDeep(fontAtlas);
    registry.SaveDirtyAssets();

    // need to move in return since return type is wrapped result
    return fontAtlas;
}

UISubsystem::UISubsystem()
    : UISubsystem(MakeHandle<UIStage>())
{
}

UISubsystem::UISubsystem(const Handle<UIStage>& uiStage)
    : m_uiStage(uiStage),
      m_uiRenderer(nullptr),
      m_wasProcessedLastFrame(false)
{
}

UISubsystem::~UISubsystem()
{

    //PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

    m_onWindowResizedHandle.Reset();
    m_onCurrentWindowChangedHandle.Reset();
}

void UISubsystem::Init()
{
    Subsystem::Init();

    Assert(m_uiStage != nullptr);

    m_uiStage->SetWorld(GetWorld());
    InitObject(m_uiStage);

    InitFont();

    const auto HandleWindowResize = [this, weakThis = MakeWeakRef(this)](Vec2i windowSize)
    {
        Handle<UISubsystem> strongThis = weakThis.Lock();

        if (!strongThis.IsValid())
        {
            HYP_LOG(UI, Warning, "UISubsystem: subsystem is expired on resize");
            return;
        }

        m_uiStage->SetSurfaceSize(windowSize);
    };

    Vec2u windowSize { 1280, 720 };

    if (g_appContext->GetMainWindow() != nullptr)
    {
        windowSize = Vec2u(g_appContext->GetMainWindow()->GetSize());

        m_onWindowResizedHandle = g_appContext->GetMainWindow()->OnWindowSizeChanged.BindThreaded(HandleWindowResize, g_simThread);
    }

    m_onCurrentWindowChangedHandle = g_appContext->OnCurrentWindowChanged.BindThreaded(
        [this, weakThis = MakeWeakRef(this), HandleWindowResize](ApplicationWindow* window)
        {
            Handle<UISubsystem> strongThis = weakThis.Lock();

            if (!strongThis.IsValid())
            {
                HYP_LOG(UI, Warning, "UISubsystem: subsystem is expired on current window changed");
                return;
            }

            if (m_onWindowResizedHandle.IsValid())
            {
                m_onWindowResizedHandle.Reset();
            }

            if (window != nullptr)
            {
                m_onWindowResizedHandle = window->OnWindowSizeChanged.BindThreaded(HandleWindowResize, g_simThread);

                HandleWindowResize(Vec2i(window->GetSize()));
            }
        },
        g_simThread);

    m_uiStage->SetSurfaceSize(Vec2i(windowSize));

    ViewDesc viewDesc {};
    viewDesc.flags = ViewFlags::UI_VIEW
        | ViewFlags::EXTERNAL_RENDERTARGET
        | (ViewFlags::DEFAULT & ~(ViewFlags::ALL_WORLD_SCENES | ViewFlags::MATCH_CAMERA_DIMENSIONS));

    viewDesc.scenes = { m_uiStage->GetScene() };
    viewDesc.camera = m_uiStage->GetCamera();
    viewDesc.entityBatchClass = UIEntityInstanceBatch::StaticClass();

    m_view = MakeHandle<View>(viewDesc);
    m_view->SetName(NAME("UISubsystem_View"));
    InitObject(m_view);
}

void UISubsystem::OnAddedToWorld()
{
    if (m_uiStage)
    {
        m_uiStage->SetWorld(GetWorld());
    }

    InitDebugOverlays();
}

void UISubsystem::OnRemovedFromWorld()
{
    if (m_uiStage)
    {
        m_uiStage->SetWorld(nullptr);
    }
}

void UISubsystem::PreUpdate(float delta)
{
}

void UISubsystem::Update(float delta)
{
    HYP_SCOPE;

    if (!cvShowDebugUI.Get())
    {
        return;
    }

    UpdateDebugOverlays();

    m_uiStage->Update(delta);

    const bool hasOtherChildUIObjects = m_uiStage->NumChildUIObjects(/* deep */ false) > m_debugOverlayContainers.Size();

    // render UI if there are non-debug overlay objects in the stage,
    // or if there are debug overlays we have to draw.
    if (hasOtherChildUIObjects || m_debugOverlays.Any())
    {
        m_view->SetOverrideCollectFunctor(ProcRef<void(RenderProxyList&)>(*this, ValueWrapper<&UISubsystem::RenderCollect>()));

        GetWorld()->ProcessViewAsync(m_view);

        m_wasProcessedLastFrame = true;
    }
    else if (m_wasProcessedLastFrame)
    {
        // just to clear it out the first time we see there are no child ui objects.
        RenderCollect(GetProducerProxyList(m_view));

        m_wasProcessedLastFrame = false;
    }
}

void UISubsystem::RenderCollect(RenderProxyList& rpl)
{
    rpl.BeginWrite();

    rpl.disableBuildRenderCollection = true;
    rpl.useOrdering = true;
    rpl.priority = m_view->GetPriority();

    rpl.meshEntityOrdering.Clear();

    rpl.GetCameras().Track(m_view->GetCamera()->Id(), m_view->GetCamera(), m_view->GetCamera()->GetRenderProxyVersionPtr());

    auto Predicate = [&rpl](UIObject* uiObject)
    {
        AssertDebug(uiObject != nullptr);

        if (uiObject->IsA(UIStage::StaticClass()) // don't render stage (too large)
            || uiObject->ComputeBlendedBackgroundColor().GetAlpha() <= 0.0001f)
        {
            // skip; not considered visible.
            return;
        }

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

        if (MaterialInstance* material = meshComponent.material)
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
    };

    m_uiStage->CollectObjects(Predicate, /* onlyVisible */ true);

    Resources::ResourceTrackerDiff meshesDiff = rpl.GetMeshEntities().GetDiff();

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

            if ((meshComponent->enableAutoInstancing || meshComponent->numInstances) && meshComponent->instanceData.IsLoaded())
            {
                const Handle<InstancedMeshData>& instancedMesh = DynamicCast<InstancedMeshData>(meshComponent->instanceData.Resolve());
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

            meshProxy.forceRebind = false;
            meshProxy.entity = MakeWeakRef(entity);
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.numIndices = meshComponent->mesh->NumIndices();
            meshProxy.numInstances = meshComponent->numInstances;
            meshProxy.enableAutoInstancing = meshComponent->enableAutoInstancing;
            meshProxy.attributes = RenderableAttributeSet(meshComponent->mesh->GetMeshAttributes(), meshComponent->material->GetAttributes());
            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
        }
    }

    rpl.EndWrite();
}

void UISubsystem::InitFont()
{
    TResult<Handle<FontAtlas>> fontAtlasResult = CreateFontAtlas();

    if (fontAtlasResult.HasError())
    {
        HYP_LOG(UI, Error, "Failed to create font atlas: {}", fontAtlasResult.GetError().GetMessage());
        return;
    }

    m_uiStage->SetDefaultFontAtlas(*fontAtlasResult);
    m_uiStage->SetTextSize(18.0f);
}

void UISubsystem::InitDebugOverlays()
{
    HYP_SCOPE;

    static constexpr UIObjectAlignment Alignments[4] = {
        UIObjectAlignment::TOP_LEFT,
        UIObjectAlignment::BOTTOM_LEFT,
        UIObjectAlignment::TOP_RIGHT,
        UIObjectAlignment::BOTTOM_RIGHT
    };

    for (int i = 0; i < 4; i++)
    {
        Handle<UIObject>& debugOverlayContainer = m_debugOverlayContainers[i];

        debugOverlayContainer = m_uiStage->CreateUIObject<UIListView>(NAME_FMT("DebugOverlay_{}", i), Vec2i::Zero(), UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
        debugOverlayContainer->SetDepth(100);
        debugOverlayContainer->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
        debugOverlayContainer->SetParentAlignment(Alignments[i]);
        debugOverlayContainer->SetOriginAlignment(Alignments[i]);
        debugOverlayContainer->SetAcceptsFocus(false); // so we dlon't steal focus from the viewport

        debugOverlayContainer->OnClick.RemoveAllDetached();
        debugOverlayContainer->OnKeyDown.RemoveAllDetached();
    }

    for (const Handle<OverlayBase>& debugOverlay : m_debugOverlays)
    {
        int placement = debugOverlay->GetPlacement();

        if (placement < 0 || placement >= int(m_debugOverlayContainers.Size()))
        {
            // Invalid placement, skip this overlay
            HYP_LOG(UI, Warning, "Invalid debug overlay placement: {}", placement);

            placement = 0; // Default to the first container
        }

        debugOverlay->Initialize(m_debugOverlayContainers[placement]);

        const Handle<UIObject>& uiObject = debugOverlay->GetUIObject();
        AssertDebug(uiObject != nullptr);

        if (uiObject != nullptr)
        {
            m_debugOverlayContainers[placement]->AddChildUIObject(uiObject);
        }
    }

    for (const Handle<UIObject>& debugOverlayContainer : m_debugOverlayContainers)
    {
        m_uiStage->AddChildUIObject(debugOverlayContainer);
    }
}

void UISubsystem::UpdateDebugOverlays()
{
    HYP_SCOPE;

    for (const Handle<OverlayBase>& debugOverlay : m_debugOverlays)
    {
        if (!debugOverlay->IsEnabled())
        {
            continue;
        }

        if (debugOverlay->GetTimer().Waiting())
        {
            continue;
        }

        debugOverlay->GetTimer().NextTick();

        debugOverlay->Update(debugOverlay->GetTimer().delta);
    }
}

void UISubsystem::AddDebugOverlay(const Handle<OverlayBase>& debugOverlay)
{
    HYP_SCOPE;

    AssertDebug(debugOverlay != nullptr);

    if (!debugOverlay)
    {
        return;
    }

    AssertOnThread(g_simThread);

    auto it = m_debugOverlays.Find(debugOverlay);

    if (it != m_debugOverlays.End())
    {
        return;
    }

    m_debugOverlays.PushBack(debugOverlay);

    int placement = debugOverlay->GetPlacement();

    if (placement < 0 || placement >= int(m_debugOverlayContainers.Size()))
    {
        // Invalid placement, skip this overlay
        HYP_LOG(UI, Warning, "Invalid debug overlay placement: {}", placement);

        placement = 0; // Default to the first container
    }

    if (!m_debugOverlayContainers[placement])
    {
        return; // not initialized yet; it'll be added later
    }

    debugOverlay->Initialize(m_uiStage);

    if (const Handle<UIObject>& object = debugOverlay->GetUIObject())
    {
        Handle<UIListViewItem> listViewItem = m_uiStage->CreateUIObject<UIListViewItem>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        //listViewItem->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
        listViewItem->AddChildUIObject(object);

        m_debugOverlayContainers[placement]->AddChildUIObject(listViewItem);
    }
}

bool UISubsystem::RemoveDebugOverlay(OverlayBase* debugOverlay)
{
    HYP_SCOPE;

    if (!debugOverlay)
    {
        return false;
    }

    AssertOnThread(g_simThread);

    auto it = m_debugOverlays.FindAs(debugOverlay);

    if (it == m_debugOverlays.End())
    {
        return false;
    }

    if (const Handle<UIObject>& object = (*it)->GetUIObject())
    {
        object->RemoveFromParent();
    }

    m_debugOverlays.Erase(it);

    return true;
}

} // namespace Hyperion
