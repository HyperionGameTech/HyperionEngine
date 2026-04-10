#include "FoxtrotGame.hpp"

#define SDL_DISABLE_OLD_NAMES

#include <SDL3/SDL.h>
#include <SDL3/SDL_revision.h>

#include <Asset/AxManager.hpp>
#include <Asset/FxConfigFile.hpp>
#include <Asset/FxSceneFile.hpp>
#include <Core/FxDefer.hpp>
#include <Core/FxPanic.hpp>
#include <Core/FxRef.hpp>
#include <Core/FxRefUtil.hpp>
#include <FxControls.hpp>
#include <FxEngine.hpp>
#include <FxMaterial.hpp>
#include <Physics/PhJolt.hpp>
#include <Renderer/Backend/RxGpuBuffer.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>
#include <Renderer/RxShadowDirectional.hpp>
#include <csignal>

FX_SET_MODULE_NAME("FoxtrotGame");

static constexpr float scMouseSensitivity = 0.25;

static constexpr uint32 scFramesForAvg = 10;


static double sClockFreq = 1.0f;

static bool sbRunning = true;

static bool sbShowShadowCam = false;

FoxtrotGame::FoxtrotGame()
{
    InitEngine();
    CreateGame();
}

void FoxtrotGame::InitEngine()
{
#ifdef FX_LOG_OUTPUT_TO_FILE
    FxLogCreateFile("FoxtrotLog.log");
#endif

    Config.Load(FX_BASE_DIR "/Config/Main.conf");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        FxModulePanic("Could not initialize SDL! (SDL err: {})\n", SDL_GetError());
    }

    // Create the global engine variables
    RxGlobals::Init();


    FxControlManager::Init();
    FxControlManager::GetInstance().OnQuit = [] { sbRunning = false; };

    // catch sigabrt to avoid macOS showing "report" popup
    signal(SIGABRT,
           [](int signum)
           {
               FxLogError("Aborted!");
               exit(1);
           });

    FxConfigEntry* window_entry = Config.GetEntry(FxHashStr64("Window"));

    const uint32 window_width = window_entry->GetMember(FxHashStr64("Width"))->Get<uint32>();
    const uint32 window_height = window_entry->GetMember(FxHashStr64("Height"))->Get<uint32>();

    FxRef<FxWindow> window = FxWindow::New(window_entry->GetMember(FxHashStr64("Title"))->Get<const char*>(),
                                           FxVec2i(window_width, window_height));

    gRenderer->SelectWindow(window);
    gRenderer->Init(FxVec2u(window_width, window_height));

    gPhysics->Create();

    gAssetManager->Start(3);

    gMaterialManager->Create();

    sClockFreq = static_cast<double>(SDL_GetPerformanceFrequency());
}

void FoxtrotGame::CreateLights()
{
    pSun = FxMakeRef<FxLightDirectional>();
    pSun->MoveTo(FxVec3f(0.5, 5, -1.0).Normalize());
    pSun->Color = FxColor::FromRGBA(0xFA, 0xD2, 0xC0, 6);
    pSun->AmbientColor = FxColor::FromRGBA(0x4A, 0x3A, 0x2A, 1);
    mMainScene.Attach(pSun);
}

void FoxtrotGame::CreateGame()
{
    mMainScene.Create();

    Player.Create();
    Player.pCamera->SetAspectRatio(gRenderer->GetWindow()->GetAspectRatio());
    // Move the player up and behind the other objects
    Player.TeleportTo(FxVec3f(0.0f, 4.0f, -4.0f));
    Player.SetFlyMode(true);

    mMainScene.SelectCamera(Player.pCamera);

    FxSceneFile scene_file;

    const char* scene_to_load = Config.GetEntry(FxHashStr64("Scene"))->Get<const char*>();

    scene_file.Load(std::format("{}/Data/{}", FX_BASE_DIR, scene_to_load), mMainScene);
    gPhysics->OptimizeBroadPhase();

    pPistolObject = mMainScene.FindObject(FxHashStr64("Pistol"));

    CreateLights();

    gShadowRenderer = new RxShadowDirectional(FxVec2u(2048, 2048));
    gShadowRenderer->ShadowCamera.ViewMatrix.LookAt(FxVec3f(0, 8, 5), FxVec3f(0.0f, 8.0f, -2.0f), FxVec3f(0, 1, 0));
    gShadowRenderer->ShadowCamera.SetFarPlane(200.0f);
    gShadowRenderer->ShadowCamera.SetNearPlane(0.1f);
    gShadowRenderer->ShadowCamera.UpdateProjectionMatrix();
    gShadowRenderer->ShadowCamera.mbRequireMatrixUpdate = false;
    gShadowRenderer->ShadowCamera.UpdateCameraMatrix();

    while (sbRunning) {
        Tick();
    }
}


static FX_FORCE_INLINE FxVec3f GetMovementVector()
{
    FxVec3f movement = FxVec3f::sZero;

    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_W)) {
        movement.Z += 1.0f;
    }
    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_S)) {
        movement.Z += -1.0f;
    }
    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_A)) {
        movement.X += 1.0f;
    }
    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_D)) {
        movement.X += -1.0f;
    }

    return movement;
}

void FoxtrotGame::ProcessControls()
{
    if (FxControlManager::IsKeyPressed(FxKey::FX_KEY_GRAVE)) {
        // Release the mouse before quitting the game incase there is a crash.
        FxControlManager::ReleaseMouse();
        sbRunning = false;
    }

    // Click to lock mouse
    if (FxControlManager::IsKeyPressed(FxKey::FX_MOUSE_LEFT) && !FxControlManager::IsMouseLocked()) {
        FxControlManager::CaptureMouse();
    }
    // Escape to unlock mouse
    else if (FxControlManager::IsKeyPressed(FxKey::FX_KEY_ESCAPE) && FxControlManager::IsMouseLocked()) {
        FxControlManager::ReleaseMouse();
    }

    if (FxControlManager::IsKeyPressed(FxKey::FX_KEY_8)) {
        sbShowShadowCam = !sbShowShadowCam;


        if (sbShowShadowCam) {
            Player.pCamera->ProjectionMatrix = gShadowRenderer->ShadowCamera.ProjectionMatrix;
            Player.pCamera->ViewMatrix = gShadowRenderer->ShadowCamera.ViewMatrix;
            Player.pCamera->UpdateCameraMatrix();
        }
        else {
            Player.pCamera->UpdateProjectionMatrix();
            Player.pCamera->UpdateCameraMatrix();
        }
    }

    if (FxControlManager::IsMouseLocked()) {
        FxVec2f mouse_delta = FxControlManager::GetMouseDelta();
        mouse_delta.X = (DeltaTime * mouse_delta.X * scMouseSensitivity);
        mouse_delta.Y = (DeltaTime * mouse_delta.Y * -scMouseSensitivity);

        // camera->Rotate(mouse_delta.GetX(), mouse_delta.GetY());
        Player.RotateHead(mouse_delta);
    }


    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_SPACE)) {
        if (!Player.IsFlyMode()) {
            Player.Jump();
        }
    }

    // Elevate up
    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_E)) {
        Player.Move(DeltaTime, FxVec3f::sUp);
    }
    // Elevate down
    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_Q)) {
        Player.Move(DeltaTime, -FxVec3f::sUp);
    }


    if (FxControlManager::IsKeyDown(FxKey::FX_KEY_LSHIFT)) {
        Player.bIsSprinting = true;
    }
    else {
        Player.bIsSprinting = false;
    }

    if (FxControlManager::IsComboDown(FxKey::FX_KEY_LSHIFT, FxKey::FX_KEY_R)) {
        // Reload the object properties from the scene
        FxSceneFile scene_file;
        scene_file.Load(
            std::format("{}/Data/{}", FX_BASE_DIR, Config.GetEntry(FxHashStr64("Scene"))->Get<const char*>()),
            mMainScene);
    }

    if (FxControlManager::IsKeyPressed(FxKey::FX_KEY_P)) {
        pHelmetObject->SetPhysicsEnabled(!pHelmetObject->GetPhysicsEnabled());
    }

    if (FxControlManager::IsKeyPressed(FxKey::FX_KEY_0)) {
        Player.SetFlyMode(true);
        Player.TeleportTo(FxVec3f(0.0f, 4.0f, -4.0f));
    }


    if (FxControlManager::IsKeyPressed(FxKey::FX_KEY_N)) {
        // Player.Physics.bDisableGravity = !Player.Physics.bDisableGravity;
        Player.SetFlyMode(!Player.IsFlyMode());
    }
}


void FoxtrotGame::Tick()
{
    const uint64 current_tick = SDL_GetPerformanceCounter();

    DeltaTime = static_cast<double>(current_tick - mLastTick) / sClockFreq;

    FrameTimeAvg += DeltaTime;

    if (!(gRenderer->GetFrameNumber() % scFramesForAvg)) {
        double frametime = FrameTimeAvg / scFramesForAvg;
        double fps = 1.0 / frametime;

        // FxLogInfo("FrameTime={}, FPS={}", frametime, fps);

        FrameTimeAvg = 0;
    }


    FxControlManager::Update();
    ProcessControls();


    if (!sbShowShadowCam) {
        Player.Move(DeltaTime, GetMovementVector());
        Player.Update(DeltaTime);
    }

    FxRef<FxPerspectiveCamera> camera = Player.pCamera;

    FxVec3f pistol_destination = camera->Position + (camera->Direction * FxVec3f(0.45)) -
                                 camera->GetRightVector() * FxVec3f(0.18) - camera->GetUpVector() * FxVec3f(0.15);

    pPistolObject->MoveTo(pistol_destination);

    PistolRotationGoal = FxQuat::FromEulerAngles(FxVec3f(-camera->mAngleY, camera->mAngleX, 0));
    pPistolObject->mRotation.SmoothInterpolate(PistolRotationGoal, 40.0, DeltaTime);

    pPistolObject->SetShadowCaster(true);

    gShadowRenderer->ShadowCamera.Position = (Player.Position + (pSun->GetPosition().Normalize() * 15.0f));

    FxVec3f target = Player.Position;

    gShadowRenderer->ShadowCamera.ViewMatrix.LookAt(gShadowRenderer->ShadowCamera.Position, target, FxVec3f(0, 1, 0));
    // FxLogInfo("{}", gShadowRenderer->ShadowCamera.ViewMatrix.Columns[3]);
    gShadowRenderer->ShadowCamera.UpdateCameraMatrix();
    gShadowRenderer->ShadowCamera.mbRequireMatrixUpdate = false;

    if (gRenderer->BeginFrame() != RxFrameResult::Success) {
        mLastTick = current_tick;
        return;
    }

    gPhysics->Update();

    RxFrameData* frame = gRenderer->GetFrame();

    frame->CommandBuffer.Reset();
    frame->CommandBuffer.Record();

    mMainScene.RenderShadows(&gShadowRenderer->ShadowCamera);
    mMainScene.Render(&gShadowRenderer->ShadowCamera);

    mLastTick = current_tick;
}

void FoxtrotGame::DestroyGame()
{
    gRenderer->GetDevice()->WaitForIdle();

    gMaterialManager->Destroy();

    delete gShadowRenderer;
    gShadowRenderer = nullptr;

    gAssetManager->Shutdown();

    gRenderer->pDeferredRenderer.DestroyRef();
}


FoxtrotGame::~FoxtrotGame()
{
    DestroyGame();
    mMainScene.Destroy();
}
