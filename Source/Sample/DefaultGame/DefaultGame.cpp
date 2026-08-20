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

#include <Core/Logging/Logger.hpp>

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
#include <UI/UITextbox.hpp>
#include <UI/Overlays/BaseStatsOverlay.hpp>
#include <UI/Overlays/StatsOverlay.hpp>
#include <UI/Overlays/ConsoleOverlay.hpp>

#include <HyperionEngine.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Game);

namespace game {

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

    virtual void BeforeConnectingToServer() override;
    virtual void AfterConnectedToServer() override;

    virtual void OnLaunch() override;
    virtual void OnUpdate(float delta) override;

    void ShowLoadingScreen();
    void HideLoadingScreen();

    void ShowConnectScreen();
    void HideConnectScreen();

    Handle<Scene> m_defaultScene;
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
    Game::OnLaunch();

    // Should be set up by Game::OnLaunch()
    Assert(m_camera.IsValid());
    
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
        // divide by 100 to get the two decimal places back:
        const float progress = float(m_syncState.progress.Get(MemoryOrder::RELAXED)) * 0.01f;
        const float rounded = MathUtil::Round(progress, 2);

        if (EngineGlobals::IsHeadless())
        {
            if (MathUtil::Fract(progress) <= 0.001f)
            {
                HYP_LOG(Game, Info, "Preparing: {}%", rounded);
            }
        }
        else
        {
            // get loading progress text UIObject
            UIStage& stage = *m_uiSubsystem->GetUIStage();

            Handle<UIObject> progressText = stage.FindChildUIObject("LoadingProgressText"_sh);
            if (progressText.IsValid())
            {
                progressText->SetText(HYP_FORMAT("{}%", rounded));
            }
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

void DefaultGame::BeforeConnectingToServer()
{
    Game::BeforeConnectingToServer();

    ShowConnectScreen();
}

void DefaultGame::AfterConnectedToServer()
{
    Game::AfterConnectedToServer();

    HideConnectScreen();
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
    if (EngineGlobals::IsHeadless())
    {
        return;
    }

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
                    errorText->SetText(String("Failed to check for content updates.\n\n")
                                         + "The error message was: " + m_syncState.lastResult.GetError().GetMessage());
                }
                else
                {
                    errorText->SetText("Content updates could not be downloaded due to an unknown error.");
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

    if (EngineGlobals::IsHeadless())
    {
        return;
    }

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

void DefaultGame::ShowConnectScreen()
{
    Assert(m_uiSubsystem.IsValid());
    if (!m_uiSubsystem.IsValid())
    {
        return;
    }

    UIStage& stage = *m_uiSubsystem->GetUIStage();

    Handle<UIObject> prevBackground = stage.FindChildUIObject("ConnectScreen_Background"_sh);
    if (prevBackground.IsValid())
    {
        stage.RemoveChildUIObject(prevBackground);
    }

    Handle<UIPanel> backgroundPanel = stage.CreateUIObject<UIPanel>(NAME("ConnectScreen_Background"), Vec2i::Zero(), UIObjectSize { { 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT } });
    backgroundPanel->SetBackgroundColor(Color(0.1f, 0.1f, 0.1f, 1.0f));
    stage.AddChildUIObject(backgroundPanel);

    Handle<UIPanel> connectPanel = backgroundPanel->CreateUIObject<UIPanel>(Vec2i::Zero(), UIObjectSize { { 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT } });
    backgroundPanel->AddChildUIObject(connectPanel);

    Handle<UIText> statusText = connectPanel->CreateUIObject<UIText>(Vec2i { 0, -40 }, UIObjectSize { { 0, UIObjectSize::AUTO }, { 80, UIObjectSize::PIXEL } });
    statusText->SetTextSize(24.0f);
    statusText->SetTextColor(Color::White());
    statusText->SetOriginAlignment(UIObjectAlignment::CENTER);
    statusText->SetParentAlignment(UIObjectAlignment::CENTER);
    connectPanel->AddChildUIObject(statusText);

    const char* cliHostAddress = EngineGlobals::GetHostAddress();
    const bool hasCliHost = cliHostAddress != nullptr && *cliHostAddress != '\0';
    const String cliHostAddressStr = hasCliHost ? String(cliHostAddress) : String::empty;

    Handle<UITextbox> hostTextbox = connectPanel->CreateUIObject<UITextbox>(Vec2i { 0, 30 }, UIObjectSize { { 300, UIObjectSize::PIXEL }, { 30, UIObjectSize::PIXEL } });
    hostTextbox->SetPlaceholder("host[:port]");
    hostTextbox->SetText(cliHostAddressStr);
    hostTextbox->SetTextSize(24.0f);
    hostTextbox->SetOriginAlignment(UIObjectAlignment::CENTER);
    hostTextbox->SetParentAlignment(UIObjectAlignment::CENTER);
    connectPanel->AddChildUIObject(hostTextbox);

    Handle<UIButton> connectButton = connectPanel->CreateUIObject<UIButton>(Vec2i { 0, 140 }, UIObjectSize { { 150, UIObjectSize::PIXEL }, { 40, UIObjectSize::PIXEL } });
    connectButton->SetText("Connect");
    connectButton->SetOriginAlignment(UIObjectAlignment::CENTER);
    connectButton->SetParentAlignment(UIObjectAlignment::CENTER);
    connectPanel->AddChildUIObject(connectButton);

    auto submitHost = [this, cliHostAddressStr, hostTextbox]()
    {
        const String hostAddress = hostTextbox->GetText();

        if (hostAddress.Empty())
        {
            return;
        }

        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [self = MakeStrongRef(this), hostAddress]()
            {
                // do next frame to reduce risk of deadlock from the Delegate (RemoveAllDetached() getting called)
                self->ConnectToServer(hostAddress);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    };

    connectButton->OnClick.Bind(connectButton,
        [submitHost](const MouseEvent&) -> UIEventHandlerResult
        {
            submitHost();

            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    if (hostTextbox.IsValid())
    {
        hostTextbox->OnTextChange.Bind(hostTextbox,
            [submitHost](const String&) -> UIEventHandlerResult
            {
                submitHost();

                return UIEventHandlerResult::STOP_BUBBLING;
            })
            .Detach();
    }

    statusText->SetText("Enter host address");

    m_connectionState.OnStateChanged.RemoveAllDetached();
    m_connectionState.OnStateChanged.Bind(
        [=](ServerConnectionState::State state)
        {
            switch (state)
            {
            case ServerConnectionState::NotStarted:
                statusText->SetText("Enter host address");
                break;
            case ServerConnectionState::Connecting: // fallthrough
            case ServerConnectionState::Connecting_StartingNextTask:
                statusText->SetText(HYP_FORMAT("Connecting to {}...", hostTextbox->GetText()));
                connectButton->SetIsVisible(false);
                
                if (hostTextbox.IsValid())
                {
                    hostTextbox->SetIsVisible(false);
                }

                break;
            case ServerConnectionState::Connected:
                statusText->SetText("Connected!");
                break;
            case ServerConnectionState::Failed:
                statusText->SetText(m_connectionState.lastResult.HasError()
                        ? (String("Failed to connect.\n\n") + m_connectionState.lastResult.GetError().GetMessage())
                        : String("Failed to connect."));

                connectButton->SetIsVisible(true);
                
                if (hostTextbox.IsValid())
                {
                    hostTextbox->SetIsVisible(true);
                }

                break;
            }
        })
        .Detach();
}

void DefaultGame::HideConnectScreen()
{
    m_connectionState.OnStateChanged.RemoveAllDetached();

    Assert(m_uiSubsystem.IsValid());
    if (!m_uiSubsystem.IsValid())
    {
        return;
    }

    UIStage& stage = *m_uiSubsystem->GetUIStage();

    Handle<UIObject> prevBackground = stage.FindChildUIObject("ConnectScreen_Background"_sh);
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
