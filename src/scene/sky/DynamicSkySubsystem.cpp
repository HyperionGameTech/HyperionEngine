/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/sky/DynamicSkySubsystem.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EntityManager.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <core/threading/Scheduler.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <util/MeshBuilder.hpp>

#include <DynamicSkySubsystem.generated.inl>

namespace hyperion {

static constexpr Vec2u DefaultSkyCubemapDimensions = Vec2u { 128, 128 };
static constexpr LockstepGameCounter::TickUnit DynamicSkyUpdateTimer = LockstepGameCounter::TickUnit(1.0f); // update every second

DynamicSkySubsystem::DynamicSkySubsystem()
    : DynamicSkySubsystem(DefaultSkyCubemapDimensions)
{
}

DynamicSkySubsystem::DynamicSkySubsystem(Vec2u dimensions)
    : m_dimensions(dimensions),
      m_updateTimer { DynamicSkyUpdateTimer }
{
}

DynamicSkySubsystem::~DynamicSkySubsystem()
{
}

void DynamicSkySubsystem::Init()
{
    { // atmospheric scattering capture setup
        m_renderScene = CreateObject<Scene>(NAME("DynamicSkyRenderScene"), SceneFlags::NONE);
        m_renderScene->SetAssetFlags(AssetObjectFlags::TRANSIENT); // don't save; it's generated at runtime
        m_renderScene->SetOwnerThreadId(g_gameThread);
        InitObject(m_renderScene);

        Handle<Node> cameraNode = m_renderScene->GetRoot()->AddChild();

        m_camera = m_renderScene->GetEntityManager()->AddEntity<Camera>(
            90.0f,
            int(m_dimensions.x), int(m_dimensions.y),
            0.1f, 10000.0f);

        m_renderScene->GetEntityManager()->AddTag<EntityTag::CAMERA_PRIMARY>(m_camera);

        m_camera->SetName(NAME("DynamicSkyCaptureCamera"));
        m_camera->SetViewMatrix(Mat4f::LookAt(Vec3f::UnitZ(), Vec3f::Zero(), Vec3f::UnitY()));
        InitObject(m_camera);
        m_renderScene->GetRoot()->AddChild(m_camera);

        auto domeNodeAsset = g_assetManager->Load<Node>("models/inv_sphere.obj");

        if (domeNodeAsset.HasValue())
        {
            Handle<Node> domeNode = domeNodeAsset->Result();
            domeNode->Scale(Vec3f(100.0f));
            domeNode->LockTransform();

            m_renderScene->GetRoot()->AddChild(domeNode);
        }
    }

    { // skybox entity setup (renders the captured texture to a box)
        m_skyboxEntity = CreateObject<Entity>();
        m_skyboxEntity->SetName(NAME("Skybox"));
        m_skyboxEntity->Scale(150.0f);
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
        mesh->SetFlags(MF_VIEW_INDEPENDENT);
        mesh->SetName(NAME("SkyboxMesh"));
        InitObject(mesh);

        MaterialAttributes materialAttributes {};
        materialAttributes.shaderDefinition = ShaderDefinition {
            NAME("Skybox"),
            ShaderProperties(mesh->GetVertexAttributes())
        };

        materialAttributes.bucket = RB_SKYBOX;
        // flip cull faces.
        materialAttributes.cullFaces = FCM_FRONT;
        // enable depth test but not write. we want skybox to be behind everything else, but rendered last to avoid overdraw.
        materialAttributes.flags = MAF_DEPTH_TEST;

        m_visScene = CreateObject<Scene>(NAME("SkyVisScene"), SceneFlags::FOREGROUND);
        m_visScene->GetRoot()->AddChild(m_skyboxEntity);

        m_envProbe = m_renderScene->GetEntityManager()->AddEntity<SkyProbe>(BoundingBox(Vec3f(-100.0f), Vec3f(100.0f)), m_dimensions);
        m_envProbe->SetEnvProbeFlags(m_envProbe->GetEnvProbeFlags() & ~EPF_PARALLAX_CORRECTED); // sky env probes are not parallax corrected, obviously
        InitObject(m_envProbe);
        m_visScene->GetRoot()->AddChild(m_envProbe);

        m_envProbe->SetReceivesUpdate(false); // we will update manually, no automatic updates

        m_envProbe->GetView()->AddScene(m_renderScene);

        Handle<Material> material = CreateObject<Material>(NAME("SkyboxMaterial"), materialAttributes);
        material->SetTexture(MaterialTextureKey::ALBEDO_MAP, m_envProbe->GetPrefilteredEnvMap());
        InitObject(material);

        // add MeshComponent to skybox entity
        m_skyboxEntity->AddComponent<MeshComponent>(MeshComponent { mesh, material });
    }
}

void DynamicSkySubsystem::OnAddedToWorld()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    GetWorld()->AddScene(m_renderScene);
    GetWorld()->AddScene(m_visScene);

    AssertDebug(m_envProbe != nullptr);
    m_envProbe->SetNeedsRender(true);
}

void DynamicSkySubsystem::OnRemovedFromWorld()
{
    GetWorld()->RemoveScene(m_renderScene);
    GetWorld()->RemoveScene(m_visScene);
}

void DynamicSkySubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
    if (scene == m_renderScene)
    {
        return;
    }

    Assert(m_skyboxEntity);
    // scene->GetRoot()->AddChild(m_skyboxEntity);
}

void DynamicSkySubsystem::OnSceneDetached(Scene* scene)
{
    if (scene == m_renderScene)
    {
        return;
    }

    // scene->GetRoot()->RemoveChild(m_skyboxEntity);
}

void DynamicSkySubsystem::Update(float delta)
{
    if (!m_envProbe)
    {
        return;
    }

    if (!m_updateTimer.Waiting())
    {
        m_updateTimer.NextTick();

        m_envProbe->Update(delta);
    }
}

} // namespace hyperion
