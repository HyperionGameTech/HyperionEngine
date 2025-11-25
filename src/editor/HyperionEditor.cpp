#include <HyperionPch.hpp>

#include <editor/HyperionEditor.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorState.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Texture.hpp>

#include <engine/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/sky/DynamicSkySubsystem.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

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

#include <core/logging/Logger.hpp>

#include <core/net/HTTPRequest.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <rendering/Mesh.hpp>

#include <lightmapper/LightmapperSubsystem.hpp>
#include <lightmapper/LightmapData.hpp>

#include <system/SystemEvent.hpp>

#include <core/config/Config.hpp>

#include <asset/ScriptAsset.hpp>

#include <HyperionEngine.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <HyperionEditor.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static const Name s_nameEditorWorld = NAME("EditorWorld");

namespace editor {

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game()
{
    m_world = CreateObject<World>(s_nameEditorWorld, WorldFlags::EDITOR_WORLD);
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::Init()
{
    Game::Init();

    m_editorSubsystem = CreateObject<EditorSubsystem>();

    GetWorld()->AddSubsystem(m_editorSubsystem);

    // GetWorld()->GetWorldGrid()->AddLayer(CreateObject<TerrainWorldGridLayer>());

#if 1
    Handle<Scene> scene = CreateObject<Scene>(NAME("MyScene"));
    m_editorSubsystem->GetCurrentProject()->GetWorld()->AddScene(scene);

    m_editorSubsystem->GetCurrentProject()->GetWorld()->AddSubsystem<DynamicSkySubsystem>();

    // add sun
    Handle<Node> sunNode = scene->GetRoot()->AddChild();
    sunNode->SetName(NAME("Sun"));

    Handle<DirectionalLight> sunEntity = scene->GetEntityManager()->AddEntity<DirectionalLight>(
        Vec3f(-0.2f, 0.8f, 0.2f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        10.0f);

    sunNode->AddChild(sunEntity);

    // Test assets
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
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

void HyperionEditor::Logic(float delta)
{
}

void HyperionEditor::OnInputEvent(const SystemEvent& event)
{
    Game::OnInputEvent(event);
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion
