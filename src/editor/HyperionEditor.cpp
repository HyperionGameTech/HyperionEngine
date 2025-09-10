#include <editor/HyperionEditor.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorState.hpp>

#include <rendering/RenderEnvironment.hpp>

// temp
#include <rendering/ParticleSystem.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/util/VoxelOctree.hpp> // temp
#include <rendering/Texture.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/SkyComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/ReflectionProbeComponent.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <asset/AssetBatch.hpp>
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

#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <system/SystemEvent.hpp>

#include <core/config/Config.hpp>

#include <asset/ScriptAsset.hpp>

#include <HyperionEngine.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static VoxelOctree* g_voxelOctree = nullptr;

namespace editor {

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game()
{
}

HyperionEditor::~HyperionEditor()
{
}

// temp
void HyperionEditor::Init()
{
    Game::Init();

#if 0
    { // script 2
        String str;
#if 0
        str = "extern class UUID {\n"
              "    ToString() -> string\n"
              "}\n"
              "extern class Vec3f {\n"
              "    x : float\n"
              "    y : float\n"
              "    z : float\n"
              "}\n"
              "extern class Name {\n"
              "    ToString() -> string\n"
              "}\n"
              "extern class Node {\n"
              "    GetName() -> Name\n"
              "}\n"
              "extern class Entity : Node {\n"
              "    GetUUID() -> UUID\n"
              "    GetWorldTranslation() -> Vec3f\n"
              "    SetWorldTranslation(translation : Vec3f) -> void\n"
              "}\n"
              "extern class Scene {\n"
              "    AddEntity(entity: Entity) -> void\n"
              "}\n"
              "testVec3 := new Vec3f()\n"
              "testVec3.x = 9.0f\n"
              "testVec4 := testVec3\n"
              "testVec4.x = 4.0f\n"
              "entity := new Entity()\n"
              "export x := (a, b) { return testVec3.x };";

#else
        // vvvvv This is a test script vvvvv
        str = "class Base {} \n"
              "class MyClass : Base {\n"
              "    blah: string = \"hello\"\n"
              "    jet : int = 1\n"
              "    x(a: int) { return a * jet; }\n"
              "    MyClass() {\n"
              "        jet = 4\n"
              "        return self\n"
              "    }\n"
              "    operator[](i: int) -> any {\n"
              "       for (j := 1; j < i; j++) {\n"
              "           if (jet > 10000.0) { break }\n"
              "           jet += j\n"
              "       }\n"
              "       return jet\n"
              "    }\n"
              "    operator[]=(i: int, val: any) -> any {\n"
              "       jet = val as int\n"
              "       return jet\n"
              "    }\n"
              "    modValue(inValue : Array<int>, farts: Fart...) -> void {\n"
              "        farts[0]\n"
              "    }\n"
              "}\n"
              "class Fart { i: int; }\n"
              "ins := new MyClass();\n"
              "testArray2 : Array<int> = [9,9,34]\n"
              "testArray2[0]++\n"
              "ref testRef := testArray2\n"
              "testRef = [4,3,13424,1]\n"
              "blah := ins.modValue(testRef, new Fart());\n"
              "getArrayElement := (elementIndex : int = 5, ary: Array<int> = [1,2,3], obj: MyClass) { return () { blah }; };\n"
              "export x := (a: float, ref b: int) { f := getArrayElement(obj : ins);\nf();\nreturn f(); };";
#endif

        ByteBuffer byteBuffer(ConstByteView(reinterpret_cast<const ubyte*>(str.Data()), reinterpret_cast<const ubyte*>(str.Data() + str.Size())));

        SourceFile sourceFile("<temp>", byteBuffer.Size());
        sourceFile.ReadIntoBuffer(byteBuffer);

        ErrorList errorList;
        Script_Instance* instance = HypScript::GetInstance().Compile(sourceFile, errorList);

        if (instance != nullptr)
        {

            InstructionStream* is = HypScript::GetInstance().Decompile(instance, &std::cout);
            delete is;

            HypScript::GetInstance().Run(instance);

            // call function
            Script_Value functionValue;
            if (HypScript::GetInstance().GetFunctionHandle(instance, "x", functionValue))
            {
                Script_Value lastReturn = HypScript::GetInstance().CallFunction(instance, functionValue, 5, 4);

                if (lastReturn.IsValid())
                {
                    HYP_LOG(Editor, Debug, "Last value : {}", lastReturn.ToString().GetData());
                }
            }
            else
            {
                HYP_LOG(Editor, Error, "Failed to get function handle for 'x'!");
            }

            HypScript::GetInstance().DestroyScript(instance);
        }
    }

    HYP_BREAKPOINT;
#endif

    m_editorSubsystem = CreateObject<EditorSubsystem>();

    GetWorld()->AddSubsystem(m_editorSubsystem);

    // if (const Handle<WorldGrid>& worldGrid = GetWorld()->GetWorldGrid())
    // {
    //     worldGrid->AddLayer(CreateObject<TerrainWorldGridLayer>());
    // }
    // else
    // {
    //     HYP_FAIL("World grid is not initialized in the editor!");
    // }

    Handle<Scene> scene = CreateObject<Scene>(SceneFlags::FOREGROUND);
    scene->SetName(NAME("myScene"));
    m_editorSubsystem->GetCurrentProject()->AddScene(scene);

    HYP_LOG(Editor, Debug, "ShaderManager memory usage: {} MiB",
        double(ShaderManager::GetInstance()->CalculateMemoryUsage()) / 1024 / 1024);

    // return;

    // auto testParticleSpawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
    //     .texture = AssetManager::GetInstance()->Load<Texture>("textures/spark.png").GetValue().Result(),
    //     .origin = Vec3f(0.0f, 6.0f, 0.0f),
    //     .startSize = 0.2f,
    //     .hasPhysics = true });
    // InitObject(testParticleSpawner);

    // scene->GetWorld()->GetRenderResource().GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(testParticleSpawner);

    // if (false)
    // { // add test area light
    //     Handle<Light> light = CreateObject<Light>(
    //         LT_AREA_RECT,
    //         Vec3f(0.0f, 1.25f, 0.0f),
    //         Vec3f(0.0f, 0.0f, -1.0f).Normalize(),
    //         Vec2f(2.0f, 2.0f),
    //         Color(1.0f, 0.0f, 0.0f),
    //         1.0f,
    //         1.0f);

    //     Handle<Texture> dummyLightTexture;

    //     if (auto dummyLightTextureAsset = AssetManager::GetInstance()->Load<Texture>("textures/brdfLUT.png"))
    //     {
    //         dummyLightTexture = dummyLightTextureAsset->Result();
    //     }

    //     light->SetMaterial(MaterialCache::GetInstance()->GetOrCreate(
    //         { .shaderDefinition = ShaderDefinition {
    //               HYP_NAME(Forward),
    //               ShaderProperties(staticMeshVertexAttributes) },
    //             .bucket = RB_OPAQUE },
    //         {}, { { MaterialTextureKey::ALBEDO_MAP, std::move(dummyLightTexture) } }));
    //     Assert(light->GetMaterial().IsValid());

    //     InitObject(light);

    //     Handle<Node> lightNode = scene->GetRoot()->AddChild();
    //     lightNode->SetName(NAME("AreaLight"));

    //     auto areaLightEntity = scene->GetEntityManager()->AddEntity();

    //     scene->GetEntityManager()->AddComponent<TransformComponent>(areaLightEntity, { Transform(light->GetPosition(), Vec3f(1.0f), Quaternion::Identity()) });

    //     scene->GetEntityManager()->AddComponent<LightComponent>(areaLightEntity, { light });

    //     lightNode->SetEntity(areaLightEntity);
    // }

// add sun
#if 1
    Handle<Node> sunNode = scene->GetRoot()->AddChild();
    sunNode->SetName(NAME("Sun"));

    Handle<DirectionalLight> sunEntity = scene->GetEntityManager()->AddEntity<DirectionalLight>(
        Vec3f(-0.2f, 0.8f, 0.2f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        9.0f);

    sunNode->AddChild(sunEntity);
#endif

    // Add Skybox
    if (true)
    {
        Handle<Entity> skyboxEntity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent<SkyComponent>(skyboxEntity, SkyComponent {});
        scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(skyboxEntity, BoundingBoxComponent { BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)) });

        Handle<Node> skydomeNode = scene->GetRoot()->AddChild();
        skydomeNode->AddChild(skyboxEntity);
        skydomeNode->SetName(NAME("Sky"));

        scene->GetEntityManager()->GetComponent<TransformComponent>(skyboxEntity) = TransformComponent { Transform(Vec3f::Zero(), Vec3f(1000.0f), Quaternion::Identity()) };
        scene->GetEntityManager()->GetComponent<VisibilityStateComponent>(skyboxEntity) = VisibilityStateComponent { VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE };
    }

#if 1
    // Test assets, nevermind this
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    // batch->Add("test_model", "models/testbed/testbed.obj");

    batch->OnComplete
        .Bind([this, scene](AssetMap& results)
            {
                Handle<Node> node = results["test_model"].ExtractAs<Node>();

                node->Scale(0.03f);
                node->SetName(NAME("test_model"));
                node->LockTransform();

                scene->GetRoot()->AddChild(node);

#if 0
                Handle<LegacyEnvGrid> envGridEntity = CreateObject<LegacyEnvGrid>(node->GetWorldAABB() * 1.2f, EnvGridOptions { EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD, Vec3u { 10, 3, 10 } });
                envGridEntity->SetName(NAME("EnvGrid2"));
                scene->GetRoot()->AddChild(envGridEntity);

                envGridEntity->AddComponent<BoundingBoxComponent>(BoundingBoxComponent { node->GetWorldAABB() * 1.2f, node->GetWorldAABB() * 1.2f });
#endif

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
                    //     meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vec4f(1.0f));
                    //     meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.1f);
                    //     meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                    //     InitObject(meshComponent->material);
                    // }

                    // zombie->AddComponent<AudioComponent>(AudioComponent { .audioSource = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav")->Result(), .playbackState = { .loopMode = AudioLoopMode::AUDIO_LOOP_MODE_ONCE, .speed = 2.0f } });

                    // temp: add test script component

                    Handle<ScriptAsset> scriptAsset = CreateObject<ScriptAsset>(NAME("NewScript"), ScriptData());

                    // register the package
                    Result assetObjectResult = g_assetManager->GetAssetRegistry()->RegisterAsset("Temp/Scripts", scriptAsset);
                    Assert(assetObjectResult, "Failed to register script asset: {}", assetObjectResult.GetError().GetMessage());

                    ResourceHandle resourceHandle(*scriptAsset->GetResource());

                    ScriptData* scriptData = scriptAsset->GetScriptData();
                    Assert(scriptData != nullptr);

                    scriptAsset->GetScriptData()->language = SL_HYPSCRIPT;
                    Memory::StrCpy(scriptData->path, "tmp.hyp", sizeof(scriptData->path));
                    Memory::StrCpy(scriptData->className, "MyClass", sizeof(scriptData->className));

                    ScriptComponent& scriptComponent = firstEntity->AddComponent<ScriptComponent>(ScriptComponent {
                        scriptAsset });

                    zombie->SetName(NAME("zombie"));

                    scene->GetRoot()->AddChild(zombie);
                }

                /// testing Voxel octree
                g_voxelOctree = new VoxelOctree();
                if (auto res = g_voxelOctree->Build(VoxelOctreeParams {}, scene->GetEntityManager()); res.HasError())
                {
                    HYP_LOG(Editor, Error, "Failed to build voxel octree for lightmapper: {}", res.GetError().GetMessage());
                }
            })
        .Detach();

    batch->LoadAsync();
#endif
}

void HyperionEditor::Logic(float delta)
{
    if (g_voxelOctree != nullptr)
    {
        DebugDrawCommandList& debugDrawCommands = g_engineDriver->GetDebugDrawer()->CreateCommandList();

        //        PerformanceClock clock;
        //        clock.Start();

        // Proc<void(const VoxelOctree&, int)> drawOctant;

        // drawOctant = [&](const VoxelOctree& octree, int depth)
        // {
        //     if (octree.GetPayload().occupiedBit)
        //     {
        //         AssertDebug(!octree.IsDivided());
        //         debugDrawCommands.box(octree.GetAABB().GetCenter(), octree.GetAABB().GetExtent(), Color::Cyan());
        //     }

        //     if (octree.IsDivided())
        //     {
        //         for (const auto& it : octree.GetOctants())
        //         {
        //             drawOctant(static_cast<const VoxelOctree&>(*it.octree), depth + 1);
        //         }
        //     }
        // };

        // drawOctant(*g_voxelOctree, 0);
        // //
        //        clock.Stop();
        //
        //        HYP_LOG_TEMP("Time to draw boxes: {}", clock.ElapsedMs());
    }
}

void HyperionEditor::OnInputEvent(const SystemEvent& event)
{
    Game::OnInputEvent(event);
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion
