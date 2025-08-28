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

#include <core/memory/pool/Pool.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/logging/Logger.hpp>

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

#if 1
    //{ // script 1
    //    // temp
    //    String str;
    //    str = "for (i := 0; i < 100; i++) { Logger.print(\"hello!\\n\"); }";

    //    ByteBuffer byteBuffer(ConstByteView(reinterpret_cast<const ubyte*>(str.Data()), reinterpret_cast<const ubyte*>(str.Data() + str.Size())));

    //    SourceFile sourceFile("<temp>", byteBuffer.Size());
    //    sourceFile.ReadIntoBuffer(byteBuffer);

    //    ErrorList errorList;
    //    ScriptHandle scriptHandle = HypScript::GetInstance().Compile(sourceFile, errorList);

    //    if (scriptHandle != INVALID_SCRIPT)
    //    {

    //        HypScript::GetInstance().Run(scriptHandle);
    //        HypScript::GetInstance().DestroyScript(scriptHandle);
    //    }
    //}
    { // script 2
        // temp
        String str;
        str = "class Base {}; \n"
              "class MyClass : Base {\n"
              "    blah: string = \"hello\";\n"
              "    func x(a: int, b: int) { return a + b; }\n"
              "    func MyClass() {\n"
              "        self.blah = \"test123\";\n"
              "    }\n"
              "};\n"
              "ins : MyClass = new MyClass();\n"
              "func modValue(ref v: int, fun: Function<int> -> int) {\nv = fun(5);\n};"
              "export func x(a: float, ref b: int) { i : int = [1,2,3][2]; };";

        ByteBuffer byteBuffer(ConstByteView(reinterpret_cast<const ubyte*>(str.Data()), reinterpret_cast<const ubyte*>(str.Data() + str.Size())));

        SourceFile sourceFile("<temp>", byteBuffer.Size());
        sourceFile.ReadIntoBuffer(byteBuffer);

        ErrorList errorList;
        ScriptHandle scriptHandle = HypScript::GetInstance().Compile(sourceFile, errorList);

        if (scriptHandle != INVALID_SCRIPT)
        {

            HypScript::GetInstance().Decompile(scriptHandle, &std::cout);

            HypScript::GetInstance().Run(scriptHandle);

            // call function
            FunctionHandle functionHandle;
            if (HypScript::GetInstance().GetFunctionHandle("x", functionHandle))
            {
                HypScript::GetInstance().CallFunction(scriptHandle, functionHandle, 5, 4);

                vm::Value lastReturn;
                HypScript::GetInstance().ReadLastReturnValue(lastReturn);

                if (lastReturn.IsValid())
                {
                    HYP_LOG(Editor, Debug, "Last value : {}", lastReturn.ToString().GetData());
                }
            }
            else
            {
                HYP_LOG(Editor, Error, "Failed to get function handle for 'x'!");
            }

            HypScript::GetInstance().DestroyScript(scriptHandle);
        }
    }
#endif

    HYP_BREAKPOINT;
    // temp

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

                    scene->GetRoot()->AddChild(zombie);

                    // if (auto* meshComponent = zombie->TryGetComponent<MeshComponent>())
                    // {
                    //     meshComponent->material = meshComponent->material->Clone();
                    //     meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vec4f(1.0f));
                    //     meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.1f);
                    //     meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                    //     InitObject(meshComponent->material);
                    // }

                    // zombie->AddComponent<AudioComponent>(AudioComponent { .audioSource = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav")->Result(), .playbackState = { .loopMode = AudioLoopMode::AUDIO_LOOP_MODE_ONCE, .speed = 2.0f } });

                    zombie->SetName(NAME("zombie"));
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
