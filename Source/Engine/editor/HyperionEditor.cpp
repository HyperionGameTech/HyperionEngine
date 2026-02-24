#include <EditorPch.hpp>

#include <editor/HyperionEditor.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorState.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/DebugDrawer.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/sky/DynamicSkySystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>
#include <ui/UIListView.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UIWindow.hpp>

#include <Core/config/Config.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/net/HTTPRequest.hpp>

#include <scripting/ScriptingService.hpp>

#include <Core/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <baking/BakerSubsystem.hpp>
#include <baking/BakeData.hpp>

#include <input/Event.hpp>

#include <system/AppContext.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <HyperionEngine.hpp>

#include <HyperionEditor.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static const Name s_nameEditorWorld = NAME("EditorWorld");

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game()
{
    m_world = MakeHandle<World>(s_nameEditorWorld, WorldFlags::EDITOR_WORLD);
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::OnLaunch_Impl()
{
    // m_editorSubsystem = MakeHandle<EditorSubsystem>();

    // GetWorld()->AddSubsystem(m_editorSubsystem);

    // GetWorld()->GetWorldGrid()->AddLayer(MakeHandle<TerrainWorldGridLayer>());

#if 1

    Handle<Scene> scene = MakeHandle<Scene>(NAME("MyScene"));
    scene->SetSceneFlags(SceneFlags::DEFAULT);
    GetWorld()->AddScene(scene);

    Handle<Camera> camera = MakeHandle<Camera>();
    camera->AddCameraController(MakeHandle<FirstPersonCameraController>());
    camera->SetName(NAME("Camera"));
    camera->SetFOV(60.0f);
    camera->SetNear(0.1f);
    camera->SetFar(3000.0f);
    camera->SetWindow(g_appContext->GetMainWindow());
    camera->SetCameraFlags(camera->GetCameraFlags() | CameraFlags::MATCH_WINDOW_SIZE);
    camera->SetDirection(Vec3f(0.0f, 0.0f, -1.0f));
    InitObject(camera);
    scene->GetRoot()->AddChild(camera);

    Handle<DirectionalLight> sun = MakeHandle<DirectionalLight>();
    sun->SetName(NAME("SunLight"));
    sun->SetDirection(Vec3f(-0.2f, 0.8f, 0.2f).Normalize());
    sun->SetIntensity(10.0f);
    sun->SetShadowMapFilter(SMF_CONTACT_HARDENED);
    InitObject(sun);
    scene->GetRoot()->AddChild(sun);

    //GetWorld()->AddSystemT<DynamicSkySystem>();

    const ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT
            | ViewFlags::GBUFFER
            | ViewFlags::MATCH_CAMERA_DIMENSIONS,
        .viewport = Viewport { .extent = Vec2u(camera->GetDimensions()), .position = Vec2i::Zero() },
        .renderTargetDesc = { .extent = Vec2u(camera->GetDimensions()) },
        .camera = camera
    };

    Handle<View> view = MakeHandle<View>(viewDesc);
    GetWorld()->AddView(view);

    // Test assets
    AssetBatch* batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    // batch->Add("test_model", "models/testbed/testbed.obj");

    batch->OnComplete
        .Bind([this, scene](AssetMap& results)
            {
                Assert(results["test_model"].IsValid());
                Assert(results["zombie"].IsValid());

                Handle<Node> node = results["test_model"].ExtractAs<Node>();

                node->Scale(0.03f);
                node->SetName(NAME("test_model"));
                node->LockTransform();

                scene->GetRoot()->AddChild(node);

                if (auto& zombieAsset = results["zombie"]; zombieAsset.IsValid())
                {
                    Handle<Node> zombie = zombieAsset.ExtractAs<Node>();
                    zombie->Scale(0.25f);
                    zombie->Translate(Vec3f(0, 2.0f, -1.0f));

                    const Handle<Entity>& firstEntity = ObjCast<Entity>(zombie->GetChild(0)->GetChild(0));
                    Assert(firstEntity != nullptr);

                    // if (auto* meshComponent = zombie->TryGetComponent<MeshComponent>())
                    // {
                    //     meshComponent->material = meshComponent->material->Clone();
                    //     meshComponent->material->SetParameter(MaterialParameterKey::MATERIAL_KEY_ALBEDO, Vec4f(1.0f));
                    //     meshComponent->material->SetParameter(MaterialParameterKey::MATERIAL_KEY_ROUGHNESS, 0.1f);
                    //     meshComponent->material->SetParameter(MaterialParameterKey::MATERIAL_KEY_METALNESS, 0.0f);
                    //     InitObject(meshComponent->material);
                    // }

                    // zombie->AddComponent<AudioComponent>(AudioComponent { .audioSource = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav")->Result(), .playbackState = { .loopMode = AudioLoopMode::ALM_ONCE, .speed = 2.0f } });

                    // temp: add test script component

                    Handle<ScriptAsset> scriptAsset = MakeHandle<ScriptAsset>(NAME("NewScript"), ScriptDesc());

                    // register the package
                    Result assetObjectResult = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Scripts", scriptAsset);
                    Assert(assetObjectResult, "Failed to register script asset: {}", assetObjectResult.GetError().GetMessage());

                    auto resGuard = scriptAsset->GetReadScope();

                    ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

                    scriptDesc.language = ScriptLanguage::HypScript;
                    Memory::StrCpy(scriptDesc.path.Data(), "FPSCounter.hyp", ArraySize(scriptDesc.path));
                    Memory::StrCpy(scriptDesc.className.Data(), "MyClass", ArraySize(scriptDesc.className));

                    ScriptComponent& scriptComponent = firstEntity->AddComponent<ScriptComponent>(ScriptComponent {
                        TAssetReference<ScriptAsset>(scriptAsset) });

                    zombie->SetName(NAME("zombie"));

                    scene->GetRoot()->AddChild(zombie);
                }
            })
        .Detach();

    batch->LoadAsync();
#endif
}

void HyperionEditor::OnUpdate_Impl(float delta)
{
}

void HyperionEditor::OnInputEvent(const Event& event)
{
    Game::OnInputEvent(event);
}

#pragma endregion HyperionEditor

} // namespace Hyperion
