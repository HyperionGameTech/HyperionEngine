#include <HyperionPch.hpp>
#include "DefaultGame.hpp"

#include <Framework/EngineGlobals.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Scene/World.hpp>
#include <Scene/Light.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Scene.hpp>
#include <Scene/View.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>

#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Scene/Components/ScriptComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Scene/Camera/FirstPersonCamera.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/Input/TouchControlsSubsystem.hpp>

#include <Lang/HypScript.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/AssetBatch.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Material.hpp>

#include <Core/Config/Config.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/TextSprite.hpp>

#include <Input/InputManager.hpp>
#include <Input/Mouse.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIStage.hpp>
#include <UI/Overlays/BaseStatsOverlay.hpp>
#include <UI/Overlays/StatsOverlay.hpp>
#include <UI/Overlays/ConsoleOverlay.hpp>

#include <HyperionEngine.hpp>

#include <DefaultGame.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Game);

namespace game {

DefaultGame::DefaultGame()
    : Game()
{
    m_packageName = NAME("DefaultGame");
}

DefaultGame::~DefaultGame()
{
}

void DefaultGame::InitializeWorld()
{
    if (!m_world.IsValid())
    {
        m_world = MakeHandle<World>(NAME("MainWorld"), WorldFlags::DEFAULT);
    }

    Game::InitializeWorld();
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
    // GetWorld()->GetWorldGrid()->AddLayer(MakeHandle<TerrainWorldGridLayer>(
    //     NAME("TerrainLayer"),
    //     WorldGridLayerInfo { Vec3f { 0.0f, -5.0f, 0.0f } }));

#if HYP_ANDROID || HYP_IOS
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
            m_defaultScene = mainScene;

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
            m_camera->SetCameraFlags(m_camera->GetCameraFlags() | CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);

            Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

            ViewDesc viewDesc {
                .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS,
                .framebufferDesc = { .extent = viewportSize },
                .camera = m_camera
            };

            Handle<View> view = MakeHandle<View>(viewDesc);
            view->SetName(NAME("DefaultGame_View"));
            GetWorld()->AddView(view);

            // auto pointLight = MakeHandle<PointLight>(Vec3f(0.0f, 7.0f, -2.0f), Color::Red(), 50.0f, 30.0f);
            // mainScene->GetRoot()->AddChild(pointLight);

            Handle<Node> textNode = MakeHandle<Node>(NAME("TextNode"));

            // add TextSprites
            Handle<TextSprite> titleSprite = MakeHandle<TextSprite>(NAME("TitleText"), "Look ma!");
            titleSprite->SetWorldTranslation(Vec3f(5.0f, 11.5f, 0.0f));
            titleSprite->SetTextColor(Color::White());
            titleSprite->SetTextSize(16.0f);
            textNode->AddChild(titleSprite);

            Handle<TextSprite> subtitleSprite = MakeHandle<TextSprite>(NAME("SubtitleText"), "No WASD!");
            subtitleSprite->SetWorldTranslation(Vec3f(5.0f, 10.0f, 0.0f));
            subtitleSprite->SetTextColor(Color(0.8f, 0.8f, 1.0f, 1.0f));
            subtitleSprite->SetTextSize(14.0f);
            textNode->AddChild(subtitleSprite);

            mainScene->GetRoot()->AddChild(textNode);

            GetWorld()->AddScene(mainScene);

            auto sunIt = mainScene->GetRoot()->GetChildren().FindIf(
                [](const Handle<Node>& child)
                {
                    return child->IsA<DirectionalLight>();
                });

            if (sunIt != mainScene->GetRoot()->GetChildren().End())
            {
                m_sun = StaticCast<DirectionalLight>(*sunIt);

                if (m_sun.IsValid())
                {
                    m_sun->SetIntensity(100.0f);
                    m_sun->SetNumShadowMapCascades(4);
                }
            }

            auto descendants = mainScene->GetRoot()->GetDescendants();

            auto zombieIt = std::find_if(
                descendants.Begin(),
                descendants.End(),
                [](Node* child)
                {
                    Entity* entity = DynamicCast<Entity>(child);
                    if (!entity)
                    {
                        return false;
                    }

                    MeshComponent* mc = entity->TryGetComponent<MeshComponent>();
                    if (!mc)
                    {
                        return false;
                    }

                    if (!mc->skeleton.IsValid())
                    {
                        return false;
                    }

                    return true;
                });

            if (zombieIt != descendants.End())
            {
                // (*zombieIt)->Remove();
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
    m_camera->SetCameraFlags(CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);
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
    Memory::CopyString(scriptDesc.path.values, "FPSCounter.hyp", ArraySize(scriptDesc.path));
    Memory::CopyString(scriptDesc.className.values, "MyClass", ArraySize(scriptDesc.className));

    Handle<Entity> cubeEnt = MakeHandle<Entity>();
    cubeEnt->SetName(NAME("Cube"));
    cubeEnt->Scale(3.0f);

    Handle<Mesh> mesh = MeshBuilder::Cube();
    mesh->SetName(NAME("CubeMesh"));
    mesh->SetIsTransient(true);

    MaterialAttributes attributes;
    attributes.shaderName = NAME("GeometryPass");
    attributes.shaderProperties = {};
    attributes.bucket = RenderBucket::Opaque;

    MaterialParameters parameters;
    parameters.roughness = 0.3f;
    parameters.metalness = 0.02f;

    Handle<Material> baseMaterial = MakeHandle<Material>(NAME("NewMat"), attributes, parameters, MaterialTextures {});
    baseMaterial->SetIsTransient(true);
    InitObject(baseMaterial);
    GetCurrentAssetRegistry()->PutAssetUnique(baseMaterial);

    Handle<Material> material = baseMaterial->Clone();
    material->SetIsTransient(true);
    InitObject(material);
    GetCurrentAssetRegistry()->PutAssetUnique(material);

    scene->GetRoot()->AddChild(cubeEnt);

    cubeEnt->Translate(Vec3f(-10.0f, 0.0f, 0.0f));

    // add MeshComponent to skybox entity
    cubeEnt->AddComponent<MeshComponent>(MeshComponent { mesh, material });

    AssetBatch* batch = g_assetManager->CreateBatch();
    batch->Add("testbed", "Models/testbed/testbed.obj");
    auto results = batch->ForceLoad();

    LoadedAsset& testbedAsset = results["testbed"];

    if (testbedAsset.IsValid())
    {
        Handle<Prefab> testbedPrefab = testbedAsset.ExtractAs<Handle<Prefab>>();

        Handle<Node> testbedNode = testbedPrefab->GetRoot()->Clone();
        testbedNode->Scale(3.0f);

        // scene->GetRoot()->AddChild(testbedNode);
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

    if (m_defaultScene.IsValid())
    {
        auto textNode = m_defaultScene->FindNodeByName("TextNode"_sh);
        if (textNode.IsValid())
        {
            for (Node* child : textNode->GetChildren())
            {
                child->Rotate(Quat4f::AxisAngles(Vec3f::UnitY(), MathUtil::DegToRad(30.0f * delta)));
            }
        }
    }

    // Rotate sun for day/night cycle
    // if (m_sun)
    //{
    //    m_sunAngle += delta * 0.5f;
    //    Vec3f dir = Vec3f(MathUtil::Sin(m_sunAngle), 0.7f, MathUtil::Cos(m_sunAngle)).Normalize();
    //    m_sun->SetDirection(dir);
    //}
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
    case EventType::CONTROLLER_BUTTON_DOWN:
        controller->GetInputHandler()->OnControllerButtonDown(event.GetControllerButton());
        break;
    case EventType::CONTROLLER_BUTTON_UP:
        controller->GetInputHandler()->OnControllerButtonUp(event.GetControllerButton());
        break;
    case EventType::CONTROLLER_ANALOG_MOVE:
    {
        const ControllerAnalogData* analogData = event.GetControllerAnalogData();

        if (analogData)
        {
            controller->GetInputHandler()->OnControllerAnalogMove(*analogData);
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
