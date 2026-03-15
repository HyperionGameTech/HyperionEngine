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
#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/sky/DynamicSkySystem.hpp>

#include <scene/components/ScriptComponent.hpp>

#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <Core/config/Config.hpp>

#include <util/MeshBuilder.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>
#include <input/Event.hpp>

#include <system/AppContext.hpp>

#include <HyperionEngine.hpp>

#include <DefaultGame.generated.inl>

namespace Hyperion {

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
    // m_editorSubsystem = MakeHandle<EditorSubsystem>();

    // GetWorld()->AddSubsystem(m_editorSubsystem);

    GetWorld()->GetWorldGrid()->AddLayer(MakeHandle<TerrainWorldGridLayer>());

    m_camera = MakeHandle<Camera>();
    m_camera->SetFOV(85.0f);
    m_camera->SetFar(10000.0f);
    m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);
    m_camera->SetWindow(g_appContext->GetMainWindow());

    InitObject(m_camera);

    Vec2u viewportSize = Vec2u(m_camera->GetDimensions());

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS,
        .renderTargetDesc = { .extent = viewportSize },
        .camera = m_camera
    };

    Handle<View> view = MakeHandle<View>(viewDesc);

    GetWorld()->AddView(view);

    Handle<Scene> scene = MakeHandle<Scene>(SceneFlags::FOREGROUND);
    scene->SetName(NAME("defaultScene"));

    scene->GetRoot()->AddChild(m_camera);

    m_defaultScene = scene;

    GetWorld()->AddScene(scene);

    // add sun
    Handle<Node> sunNode = scene->GetRoot()->AddChild();
    sunNode->SetName(NAME("Sun"));

    Handle<DirectionalLight> sunEntity = scene->GetEntityManager()->AddEntity<DirectionalLight>(
        Vec3f(-0.2f, 0.8f, 0.2f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        9.0f);

    sunNode->AddChild(sunEntity);

    // sky
    //GetWorld()->AddSystemT<DynamicSkySystem>();

    // Fps controller

    Handle<FirstPersonCameraController> cameraController = MakeHandle<FirstPersonCameraController>();
    m_camera->AddCameraController(cameraController);

    // temp: add test script component
    Handle<ScriptAsset> scriptAsset = MakeHandle<ScriptAsset>(NAME("NewScript"), ScriptDesc());

    // register the package
    Result assetObjectResult = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Scripts", scriptAsset);
    Assert(assetObjectResult, "Failed to register script asset: {}", assetObjectResult.GetError().GetMessage());

    ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

    scriptDesc.language = ScriptLanguage::HypScript;
    Memory::StrCpy(scriptDesc.path.Data(), "FPSCounter.hyp", ArraySize(scriptDesc.path));
    Memory::StrCpy(scriptDesc.className.Data(), "MyClass", ArraySize(scriptDesc.className));

    ScriptComponent& scriptComponent = sunEntity->AddComponent<ScriptComponent>(ScriptComponent {
        TAssetReference<ScriptAsset>(scriptAsset) });

    StartSimulating();
}

void DefaultGame::OnUpdate_Impl(float delta)
{
}

void DefaultGame::OnInputEvent(const Event& event)
{
    Game::OnInputEvent(event);

    CameraController* controller = m_camera->GetCameraController();

    switch (event.GetType())
    {
    case EventType::KEYUP:
        controller->GetInputHandler()->OnKeyUp(event.ToKeyboardEvent());
        break;
    case EventType::KEYDOWN:
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
}

} // namespace game
} // namespace Hyperion
