#include <HyperionPch.hpp>
#include "DefaultGame.hpp"

#include <rendering/RenderEnvironment.hpp>

#include <engine/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <rendering/Texture.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/SkyComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/ReflectionProbeComponent.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <scene/View.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/logging/Logger.hpp>
#include <util/MeshBuilder.hpp>

#include <input/InputManager.hpp>
#include <rendering/Mesh.hpp>

#include <system/SystemEvent.hpp>

#include <core/config/Config.hpp>

#include <system/AppContext.hpp>

#include <input/Mouse.hpp>
#include <HyperionEngine.hpp>
#include <engine/EngineGlobals.hpp>

#include <DefaultGame.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);
namespace game {

DefaultGame::DefaultGame()
    : Game()
{
}

DefaultGame::~DefaultGame()
{
}

void DefaultGame::OnLaunch_Impl()
{
    // m_editorSubsystem = CreateObject<EditorSubsystem>();

    // GetWorld()->AddSubsystem(m_editorSubsystem);

    GetWorld()->GetWorldGrid()->AddLayer(CreateObject<TerrainWorldGridLayer>());

    m_camera = CreateObject<Camera>();
    m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);

    InitObject(m_camera);

    Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::ENABLE_READBACK | ViewFlags::MATCH_CAMERA_DIMENSIONS,
        .viewport = Viewport { .extent = viewportSize, .position = Vec2i::Zero() },
        .outputTargetDesc = { .extent = viewportSize },
        .camera = m_camera,
        .readbackTextureFormat = TF_R10G10B10A2
    };

    Handle<View> view = CreateObject<View>(viewDesc);

    GetWorld()->AddView(view);

    Handle<Scene> scene = CreateObject<Scene>(SceneFlags::FOREGROUND);
    scene->SetName(NAME("defaultScene"));

    scene->GetRoot()->AddChild(m_camera);

    m_defaultScene = scene;

    GetWorld()->AddScene(scene);
    // m_editorSubsystem->GetCurrentProject()->AddScene(scene);

    // add sun
    Handle<Node> sunNode = scene->GetRoot()->AddChild();
    sunNode->SetName(NAME("Sun"));

    Handle<DirectionalLight> sunEntity = scene->GetEntityManager()->AddEntity<DirectionalLight>(
        Vec3f(-0.2f, 0.8f, 0.2f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        9.0f);

    sunNode->AddChild(sunEntity);

    // Add Skybox
    Handle<Entity> skyboxEntity = scene->GetEntityManager()->AddEntity();

    scene->GetEntityManager()->AddComponent<SkyComponent>(skyboxEntity, SkyComponent {});
    scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(skyboxEntity, BoundingBoxComponent { BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)) });

    Handle<Node> skydomeNode = scene->GetRoot()->AddChild();
    skydomeNode->AddChild(skyboxEntity);
    skydomeNode->SetName(NAME("Sky"));

    scene->GetEntityManager()->GetComponent<TransformComponent>(skyboxEntity) = TransformComponent { Transform(Vec3f::Zero(), Vec3f(1000.0f), Quaternion::Identity()) };
    scene->GetEntityManager()->GetComponent<VisibilityStateComponent>(skyboxEntity) = VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE };

    Handle<FirstPersonCameraController> cameraController = CreateObject<FirstPersonCameraController>();

    m_camera->AddCameraController(cameraController);

    // Test assets
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    // batch->Add("test_model", "models/sponza/sponza.obj");
    // batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    // batch->Add("test_model", "models/testbed/testbed.obj");

    batch->OnComplete
        .Bind([this, scene](AssetMap& results)
            {
                // Assert(results["test_model"].IsValid());
                // Assert(results["zombie"].IsValid());

                // Handle<Node> node = results["test_model"].ExtractAs<Node>();

                // node->Scale(0.03f);
                // node->SetName(NAME("test_model"));
                // node->LockTransform();

                // scene->GetRoot()->AddChild(node);

                // if (auto& zombieAsset = results["zombie"]; zombieAsset.IsValid())
                // {
                //     Handle<Node> zombie = zombieAsset.ExtractAs<Node>();
                //     zombie->Scale(0.25f);
                //     zombie->Translate(Vec3f(0, 2.0f, -1.0f));

                //     const Handle<Entity>& firstEntity = ObjCast<Entity>(zombie->GetChild(0)->GetChild(0));
                //     Assert(firstEntity != nullptr);

                //     // if (auto* meshComponent = zombie->TryGetComponent<MeshComponent>())
                //     // {
                //     //     meshComponent->material = meshComponent->material->Clone();
                //     //     meshComponent->material->SetParameter(MaterialParameterKey::MATERIAL_KEY_ALBEDO, Vec4f(1.0f));
                //     //     meshComponent->material->SetParameter(MaterialParameterKey::MATERIAL_KEY_ROUGHNESS, 0.1f);
                //     //     meshComponent->material->SetParameter(MaterialParameterKey::MATERIAL_KEY_METALNESS, 0.0f);
                //     //     InitObject(meshComponent->material);
                //     // }

                //     // zombie->AddComponent<AudioComponent>(AudioComponent { .audioSource = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav")->Result(), .playbackState = { .loopMode = AudioLoopMode::ALM_ONCE, .speed = 2.0f } });

                //     // temp: add test script component

                //     // Handle<ScriptAsset> scriptAsset = CreateObject<ScriptAsset>(NAME("NewScript"), ScriptData());

                //     // register the package
                //     // Result assetObjectResult = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Scripts", scriptAsset).Await();
                //     // Assert(assetObjectResult, "Failed to register script asset: {}", assetObjectResult.GetError().GetMessage());

                //     // ResourceHandle resourceHandle(*scriptAsset->GetResource());

                //     // ScriptData* scriptData = scriptAsset->GetScriptData();
                //     // Assert(scriptData != nullptr);

                //     // scriptAsset->GetScriptData()->language = SL_HYPSCRIPT;
                //     // Memory::StrCpy(scriptData->path.Data(), "tmp.hyp", ArraySize(scriptData->path));
                //     // Memory::StrCpy(scriptData->className.Data(), "MyClass", ArraySize(scriptData->className));

                //     // ScriptComponent& scriptComponent = firstEntity->AddComponent<ScriptComponent>(ScriptComponent {
                //     //     TAssetReference<ScriptAsset>(scriptAsset) });

                //     zombie->SetName(NAME("zombie"));

                //     scene->GetRoot()->AddChild(zombie);
                // }
            })
        .Detach();

    batch->LoadAsync();
}

void DefaultGame::OnUpdate_Impl(float delta)
{
}

void DefaultGame::OnInputEvent(const SystemEvent& event)
{
    Game::OnInputEvent(event);

    SystemEventType eventType = event.GetType();

    if (eventType == sys::SystemEventType::EVENT_KEYDOWN)
    {
        switch (event.GetKeyCode())
        {
        case hyperion::KeyCode::KEY_W:
            break;
        case hyperion::KeyCode::KEY_A:
            break;
        case hyperion::KeyCode::KEY_S:
            break;
        case hyperion::KeyCode::KEY_D:
            break;
        default:;
        }
    }
    else if (eventType == SystemEventType::EVENT_MOUSEBUTTON_DOWN)
    {

        m_mouseLocked = !m_mouseLocked;

        if (!m_mouseLocked)
        {
            g_inputManager->PopMouseLockState();
        }
        else
        {
            g_inputManager->PushMouseLockState(m_mouseLocked);
        }
    }

    else if (eventType == SystemEventType::EVENT_MOUSEMOTION)
    {
        m_camera->GetCameraController()->GetInputHandler()->OnMouseMove(event.ToMouseEvent());
    }

    // m_camera->GetCameraController()->GetInputHandler();
}

} // namespace game
} // namespace hyperion
