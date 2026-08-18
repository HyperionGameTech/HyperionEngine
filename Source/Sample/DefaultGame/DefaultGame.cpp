#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>

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
#include <Rendering/DebugDrawer.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/TextSprite.hpp>

#include <System/AppContext.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIStage.hpp>
#include <UI/UIText.hpp>
#include <UI/UIPanel.hpp>
#include <UI/UIListView.hpp>
#include <UI/UIButton.hpp>
#include <UI/Overlays/BaseStatsOverlay.hpp>
#include <UI/Overlays/StatsOverlay.hpp>
#include <UI/Overlays/ConsoleOverlay.hpp>

#include <HyperionEngine.hpp>

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

class DefaultGame : public Game
{
    HYP_OBJECT_BODY(DefaultGame);

public:
    DefaultGame();

    DefaultGame(const DefaultGame& other) = delete;
    DefaultGame& operator=(const DefaultGame& other) = delete;

    DefaultGame(DefaultGame&& other) noexcept = delete;
    DefaultGame& operator=(DefaultGame&& other) noexcept = delete;

    virtual ~DefaultGame() override;

protected:
    virtual void BeforeContentLoaded() override;
    virtual void AfterContentLoaded() override;

    virtual void OnSyncProgress(uint64 current, uint64 total) override;

    virtual void OnLaunch() override;
    virtual void OnUpdate(float delta) override;

    void ShowLoadingScreen();
    void HideLoadingScreen();

    Handle<Scene> m_defaultScene;
    Handle<Camera> m_camera;
    Handle<DirectionalLight> m_sun;
    float m_sunAngle = 0.0f;
};

DefaultGame::DefaultGame()
    : Game()
{
}

DefaultGame::~DefaultGame()
{
}

void DefaultGame::OnLaunch()
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

    StartSimulating();
}

void DefaultGame::OnUpdate(float delta)
{
    Game::OnUpdate(delta);

    // Loading content.
    if (IsSyncingOrPreparingContent())
    {
        // update progress text.

        // divide by 100 to get the two decimal places back:
        const float progress = float(m_syncState.progress.Get(MemoryOrder::RELAXED)) * 0.01f;
        
        // get loading progress text UIObject
        UIStage& stage = *m_uiSubsystem->GetUIStage();

        Handle<UIObject> progressText = stage.FindChildUIObject("LoadingProgressText"_sh);
        if (progressText.IsValid())
        {
            progressText->SetText(HYP_FORMAT("{}%", MathUtil::Round(progress, 2)));
        }

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

void DefaultGame::OnSyncProgress(uint64 current, uint64 total)
{
    Game::OnSyncProgress(current, total);
}

void DefaultGame::BeforeContentLoaded()
{
    Game::BeforeContentLoaded();

    ShowLoadingScreen();
}

void DefaultGame::AfterContentLoaded()
{
    Game::AfterContentLoaded();

    // Keep showing the loading screen (now displaying the error UI) if content
    // sync failed, rather than hiding it right after it was shown.
    if (m_syncState.state != ContentSyncState::Failed)
    {
        HideLoadingScreen();
    }
}

void DefaultGame::ShowLoadingScreen()
{
    Assert(m_uiSubsystem.IsValid());
    if (!m_uiSubsystem.IsValid())
    {
        return;
    }

    UIStage& stage = *m_uiSubsystem->GetUIStage();

    Handle<UIObject> prevBackground = stage.FindChildUIObject("LoadingScreen_Background"_sh);
    if (prevBackground.IsValid())
    {
        stage.RemoveChildUIObject(prevBackground);
    }

    Handle<UIPanel> backgroundPanel = stage.CreateUIObject<UIPanel>(NAME("LoadingScreen_Background"), Vec2i::Zero(), UIObjectSize { { 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT } });
    backgroundPanel->SetBackgroundColor(Color(0.1f, 0.1f, 0.1f, 1.0f));
    stage.AddChildUIObject(backgroundPanel);

    Handle<UIPanel> loadingPanel = backgroundPanel->CreateUIObject<UIPanel>(Vec2i::Zero(), UIObjectSize { { 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT } });
    backgroundPanel->AddChildUIObject(loadingPanel);

    Handle<UIText> loadingText = loadingPanel->CreateUIObject<UIText>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 80, UIObjectSize::PIXEL } });
    loadingText->SetText("Loading content...");
    loadingText->SetTextSize(24.0f);
    loadingText->SetTextColor(Color::White());
    loadingText->SetOriginAlignment(UIObjectAlignment::CENTER);
    loadingText->SetParentAlignment(UIObjectAlignment::CENTER);
    loadingPanel->AddChildUIObject(loadingText);

    Handle<UIText> loadingProgressText = loadingPanel->CreateUIObject<UIText>(NAME("LoadingProgressText"), Vec2i { 0, 100 }, UIObjectSize { { 0, UIObjectSize::AUTO }, { 80, UIObjectSize::PIXEL } });
    loadingProgressText->SetText("0%");
    loadingProgressText->SetTextSize(18.0f);
    loadingProgressText->SetTextColor(Color::White());
    loadingProgressText->SetOriginAlignment(UIObjectAlignment::CENTER);
    loadingProgressText->SetParentAlignment(UIObjectAlignment::CENTER);
    loadingPanel->AddChildUIObject(loadingProgressText);

    Handle<UIListView> errorPanel = backgroundPanel->CreateUIObject<UIListView>(Vec2i::Zero(), UIObjectSize { { 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO } });
    errorPanel->SetIsVisible(false);
    errorPanel->SetOriginAlignment(UIObjectAlignment::CENTER);
    errorPanel->SetParentAlignment(UIObjectAlignment::CENTER);
    backgroundPanel->AddChildUIObject(errorPanel);
    
    Handle<UIText> errorText = errorPanel->CreateUIObject<UIText>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO } });
    errorText->SetText("Error text");
    errorText->SetTextSize(24.0f);
    errorText->SetTextColor(Color::White());
    errorText->SetIsEnabled(false);
    errorText->SetOriginAlignment(UIObjectAlignment::CENTER);
    errorText->SetParentAlignment(UIObjectAlignment::CENTER);
    errorText->SetPadding(Vec2i { 16, 16 });
    errorPanel->AddChildUIObject(errorText);

    Handle<UIListView> buttonsListView = errorPanel->CreateUIObject<UIListView>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 80, UIObjectSize::PIXEL } });
    buttonsListView->SetTextSize(24.0f);
    buttonsListView->SetOrientation(UIListViewOrientation::HORIZONTAL);
    buttonsListView->SetIsEnabled(false);
    buttonsListView->SetOriginAlignment(UIObjectAlignment::CENTER);
    buttonsListView->SetParentAlignment(UIObjectAlignment::CENTER);
    errorPanel->AddChildUIObject(buttonsListView);

    Handle<UIButton> retryButton = buttonsListView->CreateUIObject<UIButton>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT } });
    retryButton->SetText("Retry");
    retryButton->OnClick.Bind(retryButton,
        [this](const MouseEvent&) -> UIEventHandlerResult
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [self = MakeStrongRef(this)]()
                {
                    // do next frame to reduce risk of deadlock from the Delegate (RemoveAllDetached() getting called)
                    self->SyncContentAndLaunch();
                }, TaskEnqueueFlags::FIRE_AND_FORGET);

            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    buttonsListView->AddChildUIObject(retryButton);

    Handle<UIButton> launchAnywayButton = buttonsListView->CreateUIObject<UIButton>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT } });
    launchAnywayButton->SetText("Launch Anyway");
    
    launchAnywayButton->OnClick.Bind(launchAnywayButton,
        [this, errorText](const MouseEvent&) -> UIEventHandlerResult
        {
            // Same deal as above
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [self = MakeStrongRef(this), errorText]()
            {
                if (Handle<World> world = self->LoadWorld(s_nameMainWorld); world.IsValid())
                {
                    self->m_syncState.SetState(ContentSyncState::Finished);

                    self->SetWorld(world);
                    self->Launch();
                }
                else
                {
                    errorText->SetText("Missing content required to start the game.\n"
                                        "Unable to launch.");
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    buttonsListView->AddChildUIObject(launchAnywayButton);
    
    Handle<UIButton> exitButton = buttonsListView->CreateUIObject<UIButton>(Vec2i::Zero(), UIObjectSize { { 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT } });
    exitButton->SetText("Exit");
    
    exitButton->OnClick.Bind(exitButton,
        [this](const MouseEvent&) -> UIEventHandlerResult
        {
            std::terminate();

            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    buttonsListView->AddChildUIObject(exitButton);

    m_syncState.OnStateChanged.RemoveAllDetached();
    m_syncState.OnStateChanged.Bind(
        [=](ContentSyncState::State state)
        {
            switch (state)
            {
            case ContentSyncState::NotStarted:
                loadingText->SetText("Initializing...");
                break;
            case ContentSyncState::InProgress:
                loadingText->SetText("Loading content...");
                break;
            case ContentSyncState::Downloaded_Preparing:
                loadingText->SetText("Preheating...");
                break;
            case ContentSyncState::Finished:
                loadingText->SetText("Finishing up...");
                break;
            case ContentSyncState::Failed:
                errorPanel->SetIsVisible(true);
                loadingPanel->SetIsVisible(false);

                if (m_syncState.lastResult.HasError())
                {
                    errorText->SetText(String("Content could not be downloaded.\n\n")
                                         + "The error message was: " + m_syncState.lastResult.GetError().GetMessage());
                }
                else
                {
                    errorText->SetText("Content could not be downloaded due to an unknown error.");
                }

                return;
            }

            loadingPanel->SetIsVisible(true);
            errorPanel->SetIsVisible(false);
        }).Detach();
}

void DefaultGame::HideLoadingScreen()
{
    m_syncState.OnStateChanged.RemoveAllDetached();

    Assert(m_uiSubsystem.IsValid());
    if (!m_uiSubsystem.IsValid())
    {
        return;
    }

    UIStage& stage = *m_uiSubsystem->GetUIStage();

    Handle<UIObject> prevBackground = stage.FindChildUIObject("LoadingScreen_Background"_sh);
    if (prevBackground.IsValid())
    {
        stage.RemoveChildUIObject(prevBackground);
    }
}

} // namespace game

using namespace game;

#pragma region Reflection

const Class* g_clsDefaultGame = nullptr;

const Class* DefaultGame::StaticClass()
{
    return g_clsDefaultGame;
}

// clang-format off

HYP_BEGIN_CLASS(DefaultGame, -1, 0, NAME("Game"))
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(DefaultGame);

#pragma endregion Reflection

} // namespace Hyperion
