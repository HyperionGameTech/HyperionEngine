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

#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <Lang/HypScript.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/AssetBatch.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <Core/config/Config.hpp>

#include <util/MeshBuilder.hpp>

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

static bool s_isSaving = false;

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
    //GetWorld()->GetWorldGrid()->AddLayer(MakeHandle<TerrainWorldGridLayer>());

#if 0
    auto pkg = GetCurrentAssetRegistry()->GetPackageFromPath("DefaultProject39", /* createIfNotExist */ false, /* requireLoaded */ true);
    if (pkg.IsValid())
    {
        // Get MainScene
        Handle<AssetObject> mainSceneAsset = pkg->GetAssetObject(NAME("DefaultScene1"));
        Assert(mainSceneAsset.IsValid());

        if (mainSceneAsset.IsValid())
        {
            Handle<Scene> mainScene = DynamicCast<Scene>(mainSceneAsset);
            Assert(mainScene.IsValid(), "Could not find main scene asset");
            if (mainScene.IsValid())
            {
                m_camera = DynamicCast<Camera>(mainScene->GetRoot()->GetChild(0));
                Assert(m_camera.IsValid());

                Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

                ViewDesc viewDesc {
                    .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS,
                    .framebufferDesc = { .extent = viewportSize },
                    .camera = m_camera
                };

                Handle<View> view = MakeHandle<View>(viewDesc);

                GetWorld()->AddView(view);

                GetWorld()->AddScene(mainScene);
            }
        }

        StartSimulating();
    }
    return;
#endif

#if 1
    // camera
    m_camera = MakeHandle<Camera>();
    m_camera->SetFOV(65.0f);
    m_camera->SetFarClip(1000.0f);
    m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);
    m_camera->SetWindow(g_appContext->GetMainWindow());
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

    Handle<DirectionalLight> sunEntity = scene->GetEntityManager()->AddEntity<DirectionalLight>(
        Vec3f(0.1, 0.9f, 0.1f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        10.0f);

    sunNode->AddChild(sunEntity);

    auto pointLight = MakeHandle<PointLight>(Vec3f(0.0f, 7.0f, 7.0f), Color::Red(), 5.0f, 3.0f);
    scene->GetRoot()->AddChild(pointLight);

    // temp: add test script component
    Handle<ScriptAsset> scriptAsset = MakeHandle<ScriptAsset>(NAME("NewScript"), ScriptDesc());

    // register the package
    GetCurrentAssetRegistry()->PutAsset(scriptAsset);

    ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();
    scriptDesc.language = ScriptLanguage::HypScript;
    Memory::StrCpy(scriptDesc.path.Data(), "FPSCounter.hyp", ArraySize(scriptDesc.path));
    Memory::StrCpy(scriptDesc.className.Data(), "MyClass", ArraySize(scriptDesc.className));

    AssetBatch* batch = g_assetManager->CreateBatch();
    batch->Add("testbed", "Models/SponzaGltf/Sponza.gltf");//"Models/Testbed/testbed.obj");
    auto results = batch->ForceLoad();

    LoadedAsset& testbedAsset = results["testbed"];

    if (testbedAsset.IsValid())
    {
        Handle<Node> testbedNode = testbedAsset.ExtractAs<Handle<Node>>();
        testbedNode->Scale(3.0f);

        scene->GetRoot()->AddChild(testbedNode);
    }
    else if (const AssetLoadError* error = testbedAsset.GetErrorIfFailed())
    {
        HYP_LOG(Game, Error, "Failed to load test asset: {}", error->GetMessage());
    }

    // sky
    GetWorld()->AddSystemT<DynamicSkySystem>();


    ScriptComponent& scriptComponent = sunEntity->AddComponent<ScriptComponent>(ScriptComponent {
       TAssetReference<ScriptAsset>(scriptAsset)
    });

//    Handle<FogVolume> fogVolume = MakeHandle<FogVolume>();
//    fogVolume->SetLocalBounds(BoundingBox(Vec3f(-30.0f, -0.5f, -30.0f), Vec3f(30.0f, 40.0f, 30.0f)));
//    scene->GetRoot()->AddChild(fogVolume);
//#if HYP_EDITOR
//    fogVolume->Rebake();
//#endif

    if (UISubsystem* uiSubsystem = GetUISubsystem())
    {
        uiSubsystem->AddDebugOverlay(MakeHandle<StatsOverlay>());
        uiSubsystem->AddDebugOverlay(MakeHandle<ConsoleOverlay>());
    }
#endif

    StartSimulating();
}

void DefaultGame::OnUpdate_Impl(float delta)
{
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

        if (!s_isSaving && event.GetKeyCode() == KeyCode::KEY_1)
        {
            s_isSaving = true;

            // GetCurrentAssetRegistry()->RegisterAssetsRecursively("SampleGame", BoxedValue(m_defaultScene),
            //     /* forceRelocation */ false,
            //     /* appendExistingPackagePath */ true,
            //     [](const AssetObject& obj) -> String
            //     {
            //         return HYP_FORMAT("Instances/{}", obj.InstanceClass()->GetName());
            //     });

            // // save package
            // Handle<AssetPackage> pkg = GetCurrentAssetRegistry()->GetPackageFromPath("SampleGame", /* createIfNotExist */ false, /* requireLoaded */ false);
            // if (pkg.IsValid())
            // {
            //     Result result = pkg->Save(GetLibraryDirectory());
            //     if (result.HasError())
            //     {
            //         HYP_LOG(Game, Error, "Failed to save package: {}", result.GetError().GetMessage());
            //     }

            //     s_isSaving = false;
            // }
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
    default:
        break;
    }

    return true;
}

} // namespace game
} // namespace Hyperion
