/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Scene/Camera/OrthoCamera.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Core/Threading/Scheduler.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/GameState.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <DynamicSkySystem.generated.inl>

namespace Hyperion {

extern uint32 GetFrameCounter();

static constexpr ClockTimer::TickUnit DynamicSkyUpdateTimer = ClockTimer::TickUnit(0.33f);

DynamicSkySystem::DynamicSkySystem()
    : m_updateTimer { DynamicSkyUpdateTimer },
      m_lastFrame(UINT32_MAX)
{
}

DynamicSkySystem::~DynamicSkySystem()
{
}

void DynamicSkySystem::InitializeSky()
{
    GlobalContextScope engineRegistryScope { AssetRegistryContext { GetEngineAssetRegistry() } };

    if (!m_renderScene.IsValid())
    { // atmospheric scattering capture setup
        m_renderScene = MakeHandle<Scene>(NAME("DynamicSkyRenderScene"), SceneFlags::NONE);
        m_renderScene->SetIsTransient(true); // don't save; it's generated at runtime
        m_renderScene->SetOwnerThreadId(g_simThread);
        m_renderScene->Initialize();

        Handle<Prefab> domePrefab = GetEngineAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "InvSphere"_sh);

        if (domePrefab.IsValid() && domePrefab->GetRoot().IsValid())
        {
            Handle<Node> domeNode = domePrefab->GetRoot()->Clone();
            Assert(domeNode.IsValid());

            domeNode->Scale(Vec3f(10.0f));
            domeNode->LockTransform();

            m_renderScene->GetRoot()->AddChild(domeNode);
        }
        else
        {
            HYP_LOG(Scene, Error, "Failed to load skydome model!");
        }
    }

    if (!m_visScene.IsValid())
    { // skybox entity setup (renders the captured texture to a box)
        m_skyboxEntity = MakeHandle<Entity>();
        m_skyboxEntity->SetName(NAME("Skybox"));
        m_skyboxEntity->Scale(350.0f);
        InitObject(m_skyboxEntity);

        if (VisibilityStateComponent* vis = m_skyboxEntity->TryGetComponent<VisibilityStateComponent>())
        {
            vis->flags |= VisibilityStateFlags::ALWAYS_VISIBLE;
        }
        else
        {
            m_skyboxEntity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });
        }

        Handle<Mesh> mesh = MeshBuilder::Cube();
        mesh->SetFlags(MeshFlags::ViewIndependent);
        mesh->SetName(NAME("SkyboxMesh"));
        mesh->SetIsTransient(true);
        mesh->UploadGpuData();

        MaterialAttributes materialAttributes {};
        materialAttributes.shaderName = NAME("Skybox");
        materialAttributes.bucket = RenderBucket::Sky;

        // flip cull faces.
        materialAttributes.cullFaces = FaceCullMode::Front;
        materialAttributes.blendFunction = BlendFunction::None();

        // enable depth test but not write. we want skybox to be behind everything else, but rendered last to avoid overdraw.
        materialAttributes.flags = MAF_DEPTH_TEST;

        m_visScene = MakeHandle<Scene>(NAME("SkyVisScene"), SceneFlags::FOREGROUND | SceneFlags::BACKDROP);
        m_visScene->SetIsTransient(true); // don't save; it's generated at runtime
        m_visScene->GetRoot()->AddChild(m_skyboxEntity);

        Handle<SkyProbe> skyProbe = m_renderScene->GetEntityManager()->AddEntity<SkyProbe>(
            BoundingBox::Infinity(),
            SkyProbe::DefaultDimensions);

        m_envProbe = skyProbe;

        m_envProbe->SetName(NAME("DynamicSkyProbe"));
        InitObject(m_envProbe);

        m_visScene->GetRoot()->AddChild(m_envProbe);

        m_envProbe->SetReceivesUpdate(false); // we will update manually, no automatic updates

        Handle<Material> skyboxMaterial = MakeHandle<Material>(NAME("SkyboxMaterial"), materialAttributes);
        skyboxMaterial->SetTexture(MaterialTextureKey::Diffuse, skyProbe->GetSkyboxCubemap());
        skyboxMaterial->SetIsTransient(true);
        InitObject(skyboxMaterial);

        GetCurrentAssetRegistry()->PutAssetUnique(skyboxMaterial);

        // add MeshComponent to skybox entity
        m_skyboxEntity->AddComponent<MeshComponent>(MeshComponent { mesh, skyboxMaterial });

        m_cloudEffectVolume = MakeHandle<CloudEffectVolume>();
        m_cloudEffectVolume->SetName(NAME("CloudEffectVolume"));
        m_cloudEffectVolume->SetSettings(m_world->GetEnvironmentSettings().clouds);
        InitObject(m_cloudEffectVolume);

        m_visScene->GetRoot()->AddChild(m_cloudEffectVolume);

        {   // Top-down sky visibility capture. Its matrices are rebuilt every frame by UpdateSkyVisibilityView().
            m_skyVisibilityCamera = MakeHandle<Camera>(int(SkyVisibilityMapDimensions), int(SkyVisibilityMapDimensions));
            m_skyVisibilityCamera->SetName(NAME("SkyVisibilityCamera"));
            m_skyVisibilityCamera->SetNearClip(0.0f);
            m_skyVisibilityCamera->SetFarClip(SkyVisibilityDepthRange);
            InitObject(m_skyVisibilityCamera);

            ViewDesc skyVisibilityViewDesc {};

            skyVisibilityViewDesc.flags = ViewFlags::SKY_VISIBILITY_VIEW
                | ViewFlags::ALL_FOREGROUND_SCENES
                | ViewFlags::COLLECT_ALL_ENTITIES
                | ViewFlags::SKIP_LIGHTS
                | ViewFlags::SKIP_CAMERAS
                | ViewFlags::SKIP_ENV_PROBES
                | ViewFlags::SKIP_LIGHTMAP_VOLUMES
                | ViewFlags::SKIP_PARTICLE_VOLUMES
                | ViewFlags::SKIP_FOG_VOLUMES
                | ViewFlags::SKIP_SPRITES
                | ViewFlags::SKIP_EFFECT_VOLUMES
                | ViewFlags::NO_SHADOW_VIEWS
                | ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION
                | ViewFlags::NO_ASYNC_SHADER_LOADING;

            skyVisibilityViewDesc.camera = m_skyVisibilityCamera;

            FramebufferDesc& framebufferDesc = skyVisibilityViewDesc.framebufferDesc;
            framebufferDesc.extent = Vec2u { SkyVisibilityMapDimensions, SkyVisibilityMapDimensions };

            AttachmentDesc depthAttachmentDesc {};
            depthAttachmentDesc.imageType = TextureType::Texture2D;
            depthAttachmentDesc.format = TextureFormat::D16;
            depthAttachmentDesc.loadOp = LoadOperation::Clear;
            depthAttachmentDesc.storeOp = StoreOperation::Store;
            framebufferDesc.attachments[framebufferDesc.numAttachments++] = depthAttachmentDesc;

            // depth only, and two sided so canopy cards block the sky from either side
            MaterialAttributes materialAttributes {};
            materialAttributes.shaderName = NAME("DrawShadowMap");
            materialAttributes.flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST;
            materialAttributes.cullFaces = FaceCullMode::None;

            skyVisibilityViewDesc.overrideAttributes = RenderableAttributeSet(MeshAttributes(), materialAttributes);

            m_skyVisibilityView = MakeHandle<View>(skyVisibilityViewDesc);
            m_skyVisibilityView->name = NAME("SkyVisibilityView");

            UpdateSkyVisibilityView();
        }
    }
}

void DynamicSkySystem::UpdateSkyVisibilityView()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_skyVisibilityView.IsValid() || !GetWorld())
    {
        return;
    }

    // follow the editor camera while stopped and the game camera while playing, like terrain LOD does
    const bool preferEditorViews = GetWorld()->GetGameState().IsStopped();

    Vec3f viewerPosition = Vec3f::Zero();
    bool hasPreferredViewpoint = false;
    bool hasViewpoint = false;

    for (View* view : GetWorld()->GetSimThreadViews())
    {
        if (!view || !(view->GetFlags() & ViewFlags::GBUFFER) || !view->GetCamera())
        {
            continue;
        }

        const bool isPreferred = (bool(view->GetFlags() & ViewFlags::EDITOR_VIEW) == preferEditorViews);

        if (hasViewpoint && (hasPreferredViewpoint || !isPreferred))
        {
            continue;
        }

        viewerPosition = view->GetCamera()->GetWorldTranslation();
        hasPreferredViewpoint = isPreferred;
        hasViewpoint = true;
    }

    // snapped to texels so the map doesn't shimmer as the viewer moves
    const float texelWorldSize = SkyVisibilityWorldExtent / float(SkyVisibilityMapDimensions);

    const Vec3f captureOrigin = Vec3f(
        MathUtil::Floor(viewerPosition.x / texelWorldSize) * texelWorldSize,
        viewerPosition.y + SkyVisibilityHeightAboveViewer,
        MathUtil::Floor(viewerPosition.z / texelWorldSize) * texelWorldSize);

    // looking straight down, so the up vector has to be along Z
    const Mat4f viewMatrix = Mat4f::LookAt(captureOrigin, captureOrigin - Vec3f::UnitY(), Vec3f::UnitZ());

    const float halfExtent = SkyVisibilityWorldExtent * 0.5f;
    const Mat4f projectionMatrix = Mat4f::Orthographic(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.0f, SkyVisibilityDepthRange);

    m_skyVisibilityView->cachedMatrices.view = viewMatrix;
    m_skyVisibilityView->cachedMatrices.viewProj = projectionMatrix * viewMatrix;
    m_skyVisibilityView->cachedMatrices.invProj = projectionMatrix.Inverse();
    m_skyVisibilityView->cachedFrustum.SetFromViewProjectionMatrix(m_skyVisibilityView->cachedMatrices.viewProj);

    m_skyVisibilityCamera->SetWorldTranslation(captureOrigin);
}

void DynamicSkySystem::OnAddedToWorld(World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    SystemBase::OnAddedToWorld(world);

    InitializeSky();

    GetWorld()->AddScene(m_renderScene);
    GetWorld()->AddScene(m_visScene);

    if (m_skyVisibilityView.IsValid())
    {
        GetWorld()->AddView(m_skyVisibilityView);
    }

    for (uint32 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        View* view = m_envProbe->GetView(viewIndex);

        if (view != nullptr)
        {
            view->AddScene(m_renderScene);
        }
    }

    m_onWorldEnvironmentSettingsChangedHandler = World::OnEnvironmentSettingsChanged.Bind(world, [this](const EnvironmentSettings& environmentSettings)
    {
        if (m_cloudEffectVolume.IsValid())
        {
            m_cloudEffectVolume->SetSettings(environmentSettings.clouds);
        }
    });
}

void DynamicSkySystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    m_onWorldEnvironmentSettingsChangedHandler.Reset();

    for (uint32 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        View* view = m_envProbe->GetView(viewIndex);

        if (view != nullptr)
        {
            view->RemoveScene(m_renderScene);
        }
    }

    if (m_skyVisibilityView.IsValid())
    {
        GetWorld()->RemoveView(m_skyVisibilityView);
    }

    GetWorld()->RemoveScene(m_renderScene);
    GetWorld()->RemoveScene(m_visScene);
}

void DynamicSkySystem::Process(float delta, Span<Handle<Scene>>)
{
    // advances whether or not the game is simulating, so clouds move in the editor too
    if (m_cloudEffectVolume.IsValid())
    {
        m_cloudEffectVolume->AdvanceClock(delta);
    }

    UpdateSkyVisibilityView();

    if (!m_envProbe)
    {
        return;
    }

    // Has the EnvProbe been removed from the scene?
    // This can happen if for example, the user deleted the EnvProbe in the scene hierarchy
    if (m_envProbe->GetScene() != m_visScene)
    {
        return;
    }

    // update every second OR RingBufferDepth frames (whatever is sooner)

    const uint32 currFrame = GetFrameCounter();

    // if (!m_updateTimer.Waiting() || m_lastFrame == UINT32_MAX || (currFrame - m_lastFrame) >= RingBufferDepth)
    //{
    //     m_updateTimer.NextTick();

    m_envProbe->Update(delta);

    m_lastFrame = currFrame;
    //}
}

} // namespace Hyperion
