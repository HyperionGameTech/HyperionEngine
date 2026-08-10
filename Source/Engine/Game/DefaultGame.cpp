#include <HyperionPch.hpp>
#include "DefaultGame.hpp"

#include <Framework/EngineGlobals.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Scene/World.hpp>
#include <Scene/Light.hpp>
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

#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

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

#include <System/AppContext.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIStage.hpp>
#include <UI/UIText.hpp>
#include <UI/UIPanel.hpp>
#include <UI/Overlays/BaseStatsOverlay.hpp>
#include <UI/Overlays/StatsOverlay.hpp>
#include <UI/Overlays/ConsoleOverlay.hpp>

#include <HyperionEngine.hpp>

#include <DefaultGame.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Game);

namespace game {

static Camera* FindMainCamera(World& world)
{
    for (Scene* scene : world.GetScenes())
    {
        Assert(scene != nullptr);
        
        if (scene->GetSceneFlags() & SceneFlags::FOREGROUND)
        {
            for (const Handle<Node>& node : scene->GetRoot()->GetChildren())
            {
                if (node->IsA<Camera>())
                {
                    return StaticCast<Camera>(node);
                }
            }
        }
    }
    
    return nullptr;
}

DefaultGame::DefaultGame()
    : Game()
{
}

DefaultGame::~DefaultGame()
{
}

void DefaultGame::OnLaunch_Impl()
{
    Assert(GetWorld() != nullptr);
    
    // Add a View to the World.
    // This will capture the scene(s) of the world from the camera's perspective
    // and draw it into the View's GBuffer.
    Camera* mainCamera = FindMainCamera(*GetWorld());
    if (mainCamera != nullptr)
    {
        m_camera = MakeStrongRef(mainCamera);
        m_camera->SetCameraFlags(m_camera->GetCameraFlags() | CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);

        Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

        ViewDesc viewDesc {
            .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS,
            .framebufferDesc = { .extent = viewportSize },
            .camera = m_camera
        };

        Handle<View> view = MakeHandle<View>(viewDesc);
        view->SetName(NAME_FMT("{}_View", m_camera->GetName()));
        
        GetWorld()->AddView(view);
    }
    
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
    StartSimulating();
    return;

#if 0
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

        return;
    }
#endif
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
    // #ifdef HYP_EDITOR
    //    fogVolume->Rebake();
    // #endif

#endif

    StartSimulating();
}

void DefaultGame::OnUpdate_Impl(float delta)
{
    Game::OnUpdate_Impl(delta);

    // Loading content.
    if (IsSyncingContent())
    {
        return;
    }

    // Pass touch joystick movement to all character controller input handlers
    if (TouchControlsSubsystem* tcs = GetWorld()->GetSubsystem<TouchControlsSubsystem>())
    {
        const Vec2f movementDelta = tcs->GetMovementDelta();

        for (auto [entity, controllerComp, transformComp] : GetWorld()->GetScenes()[0]->GetEntityManager()
                 ->GetEntitySet<CharacterControllerComponent, TransformComponent>().GetScopedView())
        {
            if (controllerComp.inputHandler.IsValid())
            {
                controllerComp.inputHandler->SetTouchMovementDelta(movementDelta);
            }
        }

        // Also pass to camera controller for free-camera mode
        if (m_camera)
        {
            if (CameraController* controller = m_camera->GetCameraController(); controller != nullptr && controller->GetInputHandler().IsValid())
            {
                controller->GetInputHandler()->SetTouchMovementDelta(movementDelta);
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

void DefaultGame::BeforeContentLoaded()
{
    Game::BeforeContentLoaded();

    ShowLoadingScreen();
}

void DefaultGame::AfterContentLoaded()
{
    Game::AfterContentLoaded();

    HideLoadingScreen();
}

void DefaultGame::ShowLoadingScreen()
{
    Assert(m_uiSubsystem.IsValid());
    if (!m_uiSubsystem.IsValid())
    {
        return;
    }

    UIStage& stage = *m_uiSubsystem->GetUIStage();

    Handle<UIPanel> panel = stage.CreateUIObject<UIPanel>(Vec2i::Zero(), UIObjectSize { { 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT } });
    panel->SetBackgroundColor(Color(0.1f, 0.1f, 0.1f, 1.0f));
    stage.AddChildUIObject(panel);

    Handle<UIText> loadingText = panel->CreateUIObject<UIText>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 80, UIObjectSize::PIXEL } });
    loadingText->SetText("Loading content...");
    loadingText->SetTextSize(50.0f);
    loadingText->SetTextColor(Color::White());
    loadingText->SetOriginAlignment(UIObjectAlignment::CENTER);
    loadingText->SetParentAlignment(UIObjectAlignment::CENTER);
    panel->AddChildUIObject(loadingText);
}

void DefaultGame::HideLoadingScreen()
{
    Assert(m_uiSubsystem.IsValid());
    if (!m_uiSubsystem.IsValid())
    {
        return;
    }
}

} // namespace game
} // namespace Hyperion
