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

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <DynamicSkySystem.generated.inl>

namespace Hyperion {

extern uint32 GetFrameCounter();

static constexpr Vec2u DefaultSkyCubemapDimensions = Vec2u { 128, 128 };
static constexpr ClockTimer::TickUnit DynamicSkyUpdateTimer = ClockTimer::TickUnit(1.0f); // update every second

DynamicSkySystem::DynamicSkySystem()
    : DynamicSkySystem(DefaultSkyCubemapDimensions)
{
}

DynamicSkySystem::DynamicSkySystem(Vec2u dimensions)
    : m_dimensions(dimensions),
      m_updateTimer { DynamicSkyUpdateTimer },
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

        Handle<Node> cameraNode = m_renderScene->GetRoot()->AddChild();

        m_camera = m_renderScene->GetEntityManager()->AddEntity<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            0.1f, 10000.0f);

        m_renderScene->GetEntityManager()->AddTag<EntityTag::PrimaryCamera>(m_camera);

        m_camera->SetName(NAME("DynamicSkyCaptureCamera"));
        m_camera->SetViewMatrix(Mat4f::LookAt(Vec3f::Zero(), Vec3f::UnitZ(), Vec3f::UnitY()));
        InitObject(m_camera);
        m_renderScene->GetRoot()->AddChild(m_camera);

        auto domeNodeAsset = g_assetManager->Load<Node>("Models/inv_sphere.obj");

        if (domeNodeAsset.HasValue())
        {
            Handle<Node> domeNode = domeNodeAsset->Result();
            domeNode->Scale(Vec3f(100.0f));
            domeNode->LockTransform();

            m_renderScene->GetRoot()->AddChild(domeNode);
        }
    }

    if (!m_visScene.IsValid())
    { // skybox entity setup (renders the captured texture to a box)
        m_skyboxEntity = MakeHandle<Entity>();
        m_skyboxEntity->SetName(NAME("Skybox"));
        m_skyboxEntity->Scale(150.0f);
        m_skyboxEntity->SetIsTransient(true);
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
        materialAttributes.cullFaces = FCM_FRONT;
        materialAttributes.blendFunction = BlendFunction::None();
        // enable depth test but not write. we want skybox to be behind everything else, but rendered last to avoid overdraw.
        materialAttributes.flags = MAF_DEPTH_TEST;

        m_visScene = MakeHandle<Scene>(NAME("SkyVisScene"), SceneFlags::FOREGROUND | SceneFlags::BACKDROP);
        m_visScene->SetIsTransient(true); // don't save; it's generated at runtime
        m_visScene->GetRoot()->AddChild(m_skyboxEntity);

        m_envProbe = m_renderScene->GetEntityManager()->AddEntity<SkyProbe>(BoundingBox(Vec3f(-100.0f), Vec3f(100.0f)), m_dimensions);
        m_envProbe->SetEnvProbeFlags(m_envProbe->GetEnvProbeFlags() & ~EPF_PARALLAX_CORRECTED);
        InitObject(m_envProbe);
        m_visScene->GetRoot()->AddChild(m_envProbe);

        m_envProbe->SetReceivesUpdate(false); // we will update manually, no automatic updates

        Handle<Material> skyboxMaterial = MakeHandle<Material>(NAME("SkyboxMaterial"), materialAttributes);
        skyboxMaterial->SetTexture(MaterialTextureKey::Diffuse, m_envProbe->GetPrefilteredEnvMap());
        skyboxMaterial->SetIsTransient(true);
        InitObject(skyboxMaterial);

        GetCurrentAssetRegistry()->PutAssetUnique(skyboxMaterial);

        // add MeshComponent to skybox entity
        m_skyboxEntity->AddComponent<MeshComponent>(MeshComponent { mesh, skyboxMaterial });

#if 0
        // Sky Visibility view
        m_topDownCamera = MakeHandle<Camera>();
        m_topDownCamera->SetDimensions(Vec2i { 256, 256 });
        m_topDownCamera->SetName(NAME("SkyVisbilityCamera"));
        m_topDownCamera->AddCameraController(MakeHandle<OrthoCameraController>());
        m_topDownCamera->SetDirection(Vec3f(0.0f, -1.0f, 0.0f));
        m_visScene->GetRoot()->AddChild(m_topDownCamera);

        ViewDesc topDownViewDesc {};

        topDownViewDesc.camera = m_topDownCamera;
        topDownViewDesc.flags = ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::NO_SHADOW_VIEWS
            | ViewFlags::ALL_WORLD_SCENES
            | ViewFlags::NO_FRUSTUM_CULLING; // TEMP

        FramebufferDesc& framebufferDesc = topDownViewDesc.framebufferDesc;
        framebufferDesc.extent = Vec2u { 256, 256 };

        AttachmentDesc momentsAttachmentDesc {};
        momentsAttachmentDesc.imageType = TextureType::Texture2D;
        momentsAttachmentDesc.format = TextureFormat::RG16F;
        framebufferDesc.attachments[framebufferDesc.numAttachments++] = momentsAttachmentDesc;

        AttachmentDesc depthAttachmentDesc {};
        depthAttachmentDesc.imageType = TextureType::Texture2D;
        depthAttachmentDesc.format = TextureFormat::D16;
        framebufferDesc.attachments[framebufferDesc.numAttachments++] = depthAttachmentDesc;

        m_topDownView = MakeHandle<View>(topDownViewDesc);
#endif
    }
}

void DynamicSkySystem::OnAddedToWorld(World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    SystemBase::OnAddedToWorld(world);

    InitializeSky();

    GetWorld()->AddScene(m_renderScene);
    GetWorld()->AddScene(m_visScene);

    if (m_topDownView.IsValid())
    {
        GetWorld()->AddView(m_topDownView);
    }

    for (uint32 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        View* view = m_envProbe->GetView(viewIndex);

        if (view != nullptr)
        {
            view->AddScene(m_renderScene);
        }
    }
}

void DynamicSkySystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    for (uint32 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        View* view = m_envProbe->GetView(viewIndex);

        if (view != nullptr)
        {
            view->RemoveScene(m_renderScene);
        }
    }

    if (m_topDownView.IsValid())
    {
        GetWorld()->RemoveView(m_topDownView);
    }

    GetWorld()->RemoveScene(m_renderScene);
    GetWorld()->RemoveScene(m_visScene);
}

void DynamicSkySystem::Process(float delta, Span<Handle<Scene>>)
{
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
