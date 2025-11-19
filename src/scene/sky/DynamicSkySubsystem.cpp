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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineGlobals.hpp>

#include <DynamicSkySubsystem.generated.inl>

namespace hyperion {

static constexpr Vec2u DefaultSkyCubemapDimensions = Vec2u { 1024, 1024 };

DynamicSkySubsystem::DynamicSkySubsystem()
    : DynamicSkySubsystem(DefaultSkyCubemapDimensions)
{
}

DynamicSkySubsystem::DynamicSkySubsystem(Vec2u dimensions)
    : m_dimensions(dimensions)
{
}

DynamicSkySubsystem::~DynamicSkySubsystem()
{
}

void DynamicSkySubsystem::Init()
{
    m_cubemap = CreateObject<Texture>(TextureDesc {
        TT_CUBEMAP,
        TF_R11G11B10F,
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR });

    m_cubemap->SetName(NAME("Skydome_Cubemap"));
    m_cubemap->SetAssetFlags(AssetObjectFlags::TRANSIENT); // don't save; it's generated at runtime

    g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_cubemap);
    InitObject(m_cubemap);

    { // sky rendering stuff
        m_renderScene = CreateObject<Scene>(NAME("DynamicSkyRenderScene"), SceneFlags::HAS_OCTREE);
        m_renderScene->SetAssetFlags(AssetObjectFlags::TRANSIENT); // don't save; it's generated at runtime
        m_renderScene->SetOwnerThreadId(g_gameThread);
        InitObject(m_renderScene);

        Handle<Node> cameraNode = m_renderScene->GetRoot()->AddChild();

        m_camera = m_renderScene->GetEntityManager()->AddEntity<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            0.1f, 10000.0f);

        m_renderScene->GetEntityManager()->AddTag<EntityTag::CAMERA_PRIMARY>(m_camera);

        m_camera->SetName(NAME("DynamicSkyCaptureCamera"));
        m_camera->SetViewMatrix(Mat4f::LookAt(Vec3f::UnitZ(), Vec3f::Zero(), Vec3f::UnitY()));

        InitObject(m_camera);

        cameraNode->AddChild(m_camera);
        cameraNode->SetName(m_camera->GetName());

        m_envProbe = m_renderScene->GetEntityManager()->AddEntity<SkyProbe>(BoundingBox(Vec3f(-100.0f), Vec3f(100.0f)), m_dimensions);
        m_envProbe->SetEnvProbeFlags(m_envProbe->GetEnvProbeFlags() & ~EPF_PARALLAX_CORRECTED); // sky env probes are not parallax corrected, obviously

        Handle<Node> envProbeNode = m_renderScene->GetRoot()->AddChild();
        envProbeNode->AddChild(m_envProbe);
        InitObject(m_envProbe);

        auto domeNodeAsset = g_assetManager->Load<Node>("models/inv_sphere.obj");

        if (domeNodeAsset.HasValue())
        {
            Handle<Node> domeNode = domeNodeAsset->Result();
            domeNode->Scale(Vec3f(10.0f));
            domeNode->LockTransform();

            m_renderScene->GetRoot()->AddChild(domeNode);
        }
    }

    {
        m_visScene = CreateObject<Scene>(NAME("SkyVisScene"), SceneFlags::FOREGROUND);
        m_visScene->SetAssetFlags(AssetObjectFlags::TRANSIENT); // don't save; it's generated at runtime

        m_skyboxEntity = m_visScene->GetEntityManager()->AddEntity();
        m_visScene->GetEntityManager()->AddComponent<BoundingBoxComponent>(m_skyboxEntity, BoundingBoxComponent { BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)) });
        m_visScene->GetEntityManager()->GetComponent<TransformComponent>(m_skyboxEntity) = TransformComponent { Transform(Vec3f::Zero(), Vec3f(1000.0f), Quaternion::Identity()) };
        m_visScene->GetEntityManager()->GetComponent<VisibilityStateComponent>(m_skyboxEntity) = VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE };

        Handle<Mesh> mesh;
        Handle<Material> material;

        if (!mesh.IsValid())
        {
            mesh = MeshBuilder::Cube();
            mesh->SetFlags(MF_VIEW_INDEPENDENT);
            mesh->SetName(NAME("Skybox_Mesh"));

            g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Meshes/Skydome", mesh->GetAsset());

            InitObject(mesh);
        }

        if (!material)
        {
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

            material = CreateObject<Material>(NAME("SkyboxMaterial"), materialAttributes);
            material->SetTexture(MaterialTextureKey::ALBEDO_MAP, m_cubemap);

            InitObject(material);
        }

        // add MeshComponent to skybox entity
        m_visScene->GetEntityManager()->AddComponent<MeshComponent>(m_skyboxEntity, MeshComponent { mesh, material });
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

void DynamicSkySubsystem::Update(float delta)
{
}

} // namespace hyperion
