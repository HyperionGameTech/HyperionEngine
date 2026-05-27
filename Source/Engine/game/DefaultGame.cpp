#include <HyperionPch.hpp>
#include "DefaultGame.hpp"

#include <engine/EngineGlobals.hpp>
#include <rendering/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Scene.hpp>
#include <scene/View.hpp>
#include <scene/FogVolume.hpp>
#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/sky/DynamicSkySystem.hpp>

#include <scene/components/ScriptComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <scene/input/TouchControlsSubsystem.hpp>

#include <Lang/HypScript.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/AssetBatch.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>

#include <Core/config/Config.hpp>

#include <rendering/util/MeshBuilder.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>
#include <input/Event.hpp>

#include <system/AppContext.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>
#include <ui/overlays/BaseStatsOverlay.hpp>
#include <ui/overlays/StatsOverlay.hpp>
#include <ui/overlays/ConsoleOverlay.hpp>

#include <HyperionEngine.hpp>

#include <DefaultGame.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Game);

namespace game {

DefaultGame::DefaultGame()
    : Game()
{
    m_world = MakeHandle<World>(NAME("MainWorld"), WorldFlags::DEFAULT);
}

DefaultGame::~DefaultGame()
{
}

void DefaultGame::OnLaunch_Impl()
{
    if (UISubsystem* uiSubsystem = GetUISubsystem())
    {
        uiSubsystem->AddDebugOverlay(MakeHandle<StatsOverlay>());
        uiSubsystem->AddDebugOverlay(MakeHandle<ConsoleOverlay>());
    }

    // sky
    GetWorld()->AddSystemT<DynamicSkySystem>();
    // GetWorld()->GetWorldGrid()->AddLayer(MakeHandle<TerrainWorldGridLayer>());

#ifdef HYP_ANDROID
    GetWorld()->AddSubsystem(MakeHandle<TouchControlsSubsystem>());
#endif

#if 1
    // Get MainScene
    Handle<AssetObject> mainSceneAsset = GetCurrentAssetRegistry()->GetAsset<Scene>(AssetBuckets::Scenes, "MainScene"_sh);
    Assert(mainSceneAsset.IsValid());

    if (mainSceneAsset.IsValid())
    {
        Handle<Scene> mainScene = DynamicCast<Scene>(mainSceneAsset);
        Assert(mainScene.IsValid(), "Could not find main scene asset");
        if (mainScene.IsValid())
        {
            for (const Handle<Node>& node : mainScene->GetRoot()->GetChildren())
            {
                if (node->IsA<Camera>())
                {
                    m_camera = StaticCast<Camera>(node);
                    break;
                }
            }

            Assert(m_camera.IsValid());
            m_camera->SetWorldTranslation(Vec3f(0.0f, 5.0f, 3.0f));
            m_camera->SetCameraFlags(m_camera->GetCameraFlags() | CameraFlags::MATCH_WINDOW_SIZE);

            Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

            ViewDesc viewDesc {
                .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS,
                .framebufferDesc = { .extent = viewportSize },
                .camera = m_camera
            };

            Handle<View> view = MakeHandle<View>(viewDesc);

            GetWorld()->AddView(view);

            auto pointLight = MakeHandle<PointLight>(Vec3f(0.0f, 7.0f, -2.0f), Color::Red(), 50.0f, 30.0f);
            mainScene->GetRoot()->AddChild(pointLight);

            GetWorld()->AddScene(mainScene);

            auto sunIt = mainScene->GetRoot()->GetChildren().FindIf([](const Handle<Node>& child)
                {
                    return child->IsA<DirectionalLight>();
                });
            if (sunIt != mainScene->GetRoot()->GetChildren().End())
            {
                m_sun = StaticCast<DirectionalLight>(*sunIt);
            }
        }

        StartSimulating();

        return;
    }
#endif

#if 1
    // camera
    m_camera = MakeHandle<Camera>();
    m_camera->SetFOV(65.0f);
    m_camera->SetNearClip(0.1f);
    m_camera->SetFarClip(1000.0f);
    m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);
    m_camera->AddTag<EntityTag::PrimaryCamera>();

    InitObject(m_camera);

    // Fps controller
    Handle<FirstPersonCameraController> cameraController = MakeHandle<FirstPersonCameraController>();
    m_camera->AddCameraController(cameraController);

    // view
    Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS,
        .framebufferDesc = { .extent = viewportSize },
        .camera = m_camera
    };

    Handle<View> view = MakeHandle<View>(viewDesc);
    GetWorld()->AddView(view);

    Handle<Scene> scene = MakeHandle<Scene>(SceneFlags::FOREGROUND);
    scene->SetName(NAME("MainScene"));

    scene->GetRoot()->AddChild(m_camera);

    m_defaultScene = scene;

    GetWorld()->AddScene(scene);

    GetCurrentAssetRegistry()->PutAsset(scene);

    // add sun
    Handle<Node> sunNode = scene->GetRoot()->AddChild();
    sunNode->SetName(NAME("Sun"));

    m_sun = scene->GetEntityManager()->AddEntity<DirectionalLight>(
        Vec3f(-0.5f, 0.4f, 0.1f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        10.0f);

    sunNode->AddChild(m_sun);

    auto pointLight = MakeHandle<PointLight>(Vec3f(-10.0f, 4.0f, 0.0f), Color::Red(), 50.0f, 30.0f);
    scene->GetRoot()->AddChild(pointLight);

    // temp: add test script component
    Handle<ScriptAsset> scriptAsset = MakeHandle<ScriptAsset>(NAME("FPSCounter"), ScriptDesc());

    // register the package
    GetCurrentAssetRegistry()->PutAsset(scriptAsset);

    ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();
    scriptDesc.language = ScriptLanguage::HypScript;
    Memory::StrCpy(scriptDesc.path.Data(), "FPSCounter.hyp", ArraySize(scriptDesc.path));
    Memory::StrCpy(scriptDesc.className.Data(), "MyClass", ArraySize(scriptDesc.className));

    Handle<Entity> cubeEnt = MakeHandle<Entity>();
    cubeEnt->SetName(NAME("Cube"));
    cubeEnt->Scale(3.0f);

    Handle<Mesh> mesh = MeshBuilder::Cube();
    mesh->SetFlags(MeshFlags::ViewIndependent);
    mesh->SetName(NAME("CubeMesh"));
    mesh->SetIsTransient(true);
    InitObject(mesh);

    MaterialAttributes attributes;
    attributes.shaderName = NAME("GeometryPass");
    attributes.shaderProperties = {};
    attributes.bucket = RenderBucket::Opaque;

    MaterialParameters parameters;
    parameters.roughness = 0.3f;
    parameters.metalness = 0.02f;

    Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(NAME("NewMat"), attributes, parameters, MaterialTextures {});
    materialDefinition->SetIsTransient(true);
    InitObject(materialDefinition);
    GetCurrentAssetRegistry()->PutAssetUnique(materialDefinition);

    Handle<MaterialInstance> materialInstance = MakeHandle<MaterialInstance>(NAME("NewMat"), materialDefinition);
    materialInstance->SetIsTransient(true);
    InitObject(materialInstance);
    GetCurrentAssetRegistry()->PutAssetUnique(materialInstance);

    scene->GetRoot()->AddChild(cubeEnt);

    cubeEnt->Translate(Vec3f(-10.0f, 0.0f, 0.0f));

    // add MeshComponent to skybox entity
    cubeEnt->AddComponent<MeshComponent>(MeshComponent { mesh, materialInstance });

     AssetBatch* batch = g_assetManager->CreateBatch();
     batch->Add("testbed", "Models/testbed/testbed.obj");
     auto results = batch->ForceLoad();

     LoadedAsset& testbedAsset = results["testbed"];

     if (testbedAsset.IsValid())
     {
         Handle<Node> testbedNode = testbedAsset.ExtractAs<Handle<Node>>();
         testbedNode->Scale(3.0f);

         //scene->GetRoot()->AddChild(testbedNode);
     }
     else if (const AssetLoadError* error = testbedAsset.GetErrorIfFailed())
     {
         HYP_LOG(Game, Error, "Failed to load test asset: {}", error->GetMessage());
     }

    ScriptComponent& scriptComponent = m_sun->AddComponent<ScriptComponent>(ScriptComponent {
        scriptAsset });

    //    Handle<FogVolume> fogVolume = MakeHandle<FogVolume>();
    //    fogVolume->SetLocalBounds(BoundingBox(Vec3f(-30.0f, -0.5f, -30.0f), Vec3f(30.0f, 40.0f, 30.0f)));
    //    scene->GetRoot()->AddChild(fogVolume);
    // #if HYP_EDITOR
    //    fogVolume->Rebake();
    // #endif

#endif

    StartSimulating();
}

void DefaultGame::OnUpdate_Impl(float delta)
{
    // Pass joystick movement to camera controller
    if (m_camera)
    {
        if (CameraController* controller = m_camera->GetCameraController())
        {
            if (TouchControlsSubsystem* tcs = GetWorld()->GetSubsystem<TouchControlsSubsystem>())
            {
                controller->GetInputHandler()->SetTouchMovementDelta(tcs->GetMovementDelta());
            }
        }
    }

    // Rotate sun for day/night cycle
    if (m_sun)
    {
        m_sunAngle += delta * 0.5f;
        Vec3f dir = Vec3f(MathUtil::Sin(m_sunAngle), 0.4f, MathUtil::Cos(m_sunAngle)).Normalize();
        m_sun->SetDirection(dir);
    }
}

bool DefaultGame::OnInputEvent(const Event& event)
{
    if (Game::OnInputEvent(event))
    {
        if (GetUISubsystem()->GetUIStage()->HasFocus())
        {
            return true;
        }
    }

    if (!m_camera)
    {
        return false;
    }

    CameraController* controller = m_camera->GetCameraController();

    if (!controller)
    {
        return false;
    }

    switch (event.GetType())
    {
    case EventType::KEYUP:
        controller->GetInputHandler()->OnKeyUp(event.ToKeyboardEvent());
        break;
    case EventType::KEYDOWN:
        if (event.GetKeyCode() == KeyCode::KEY_TILDE)
        {
            break;
        }

        controller->GetInputHandler()->OnKeyDown(event.ToKeyboardEvent());
        break;
    case EventType::MOUSEBUTTON_DOWN:
        controller->GetInputHandler()->OnMouseDown(event.ToMouseEvent());
        break;
    case EventType::MOUSEBUTTON_UP:
        controller->GetInputHandler()->OnMouseUp(event.ToMouseEvent());
        break;
    case EventType::MOUSEMOTION:
        controller->GetInputHandler()->OnMouseMove(event.ToMouseEvent());
        break;
    case EventType::TOUCH_DOWN:
    {
        TouchEvent touchEvent = event.ToTouchEvent();

        if (TouchControlsSubsystem* tcs = GetWorld()->GetSubsystem<TouchControlsSubsystem>())
        {
            TouchPoint touchPoint;
            if (tcs->GetTouchPoint(touchEvent.pointerId, touchPoint) && !touchPoint.isLeftSide)
            {
                controller->GetInputHandler()->OnTouchDown(touchEvent);
            }
        }
        break;
    }
    case EventType::TOUCH_UP:
    {
        TouchEvent touchEvent = event.ToTouchEvent();

        if (TouchControlsSubsystem* tcs = GetWorld()->GetSubsystem<TouchControlsSubsystem>())
        {
            TouchPoint touchPoint;
            if (tcs->GetTouchPoint(touchEvent.pointerId, touchPoint) && !touchPoint.isLeftSide)
            {
                controller->GetInputHandler()->OnTouchUp(touchEvent);
            }
        }
        break;
    }
    case EventType::TOUCH_MOVE:
    {
        TouchEvent touchEvent = event.ToTouchEvent();

        if (TouchControlsSubsystem* tcs = GetWorld()->GetSubsystem<TouchControlsSubsystem>())
        {
            TouchPoint touchPoint;
            if (tcs->GetTouchPoint(touchEvent.pointerId, touchPoint) && !touchPoint.isLeftSide)
            {
                controller->GetInputHandler()->OnTouchMove(touchEvent);
            }
        }
        break;
    }
    default:
        break;
    }

    return true;
}

} // namespace game
} // namespace Hyperion
