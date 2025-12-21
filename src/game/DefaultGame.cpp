#include <HyperionPch.hpp>
#include "DefaultGame.hpp"

#include <engine/EngineGlobals.hpp>
#include <engine/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Scene.hpp>
#include <scene/View.hpp>
#include <scene/EntityManager.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <asset/ScriptAsset.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <util/MeshBuilder.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>
#include <input/Event.hpp>

#include <core/config/Config.hpp>

#include <system/AppContext.hpp>

#include <HyperionEngine.hpp>

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
    m_camera->SetFOV(85.0f);
    m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);
    m_camera->SetWindow(g_appContext->GetMainWindow());

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

    Handle<FirstPersonCameraController> cameraController = CreateObject<FirstPersonCameraController>();
    m_camera->AddCameraController(cameraController);

    // temp: add test script component

    Handle<ScriptAsset> scriptAsset = CreateObject<ScriptAsset>(NAME("NewScript"), ScriptData());

    // register the package
    Result assetObjectResult = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Scripts", scriptAsset).Await();
    Assert(assetObjectResult, "Failed to register script asset: {}", assetObjectResult.GetError().GetMessage());

    ResourceHandle resourceHandle(*scriptAsset->GetResource());

    ScriptData* scriptData = scriptAsset->GetScriptData();
    Assert(scriptData != nullptr);

    scriptAsset->GetScriptData()->language = SL_HYPSCRIPT;
    Memory::StrCpy(scriptData->path.Data(), "tmp.hyp", ArraySize(scriptData->path));
    Memory::StrCpy(scriptData->className.Data(), "MyClass", ArraySize(scriptData->className));

    ScriptComponent& scriptComponent = sunEntity->AddComponent<ScriptComponent>(ScriptComponent {
        TAssetReference<ScriptAsset>(scriptAsset) });

    GetWorld()->StartSimulating();
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
} // namespace hyperion
