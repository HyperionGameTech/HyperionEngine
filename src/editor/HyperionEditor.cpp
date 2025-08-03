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

#include <scripting/Script.hpp>
#include <scripting/ScriptingService.hpp>

#include <core/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <rendering/Mesh.hpp>

#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <system/SystemEvent.hpp>

#include <core/config/Config.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>
#include <EngineGlobals.hpp>
#include <Engine.hpp>

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

    m_editorSubsystem = CreateObject<EditorSubsystem>(GetAppContext());

    g_engine->GetWorld()->AddSubsystem(m_editorSubsystem);

    // if (const Handle<WorldGrid>& worldGrid = g_engine->GetWorld()->GetWorldGrid())
    // {
    //     // // Initialize the world grid subsystem
    //     // worldGrid->AddPlugin(0, MakeRefCountedPtr<TerrainWorldGridPlugin>());

    //     worldGrid->AddLayer(CreateObject<TerrainWorldGridLayer>());
    // }
    // else
    // {
    //     HYP_FAIL("World grid is not initialized in the editor!");
    // }

    Handle<Scene> scene = CreateObject<Scene>(SceneFlags::FOREGROUND);
    m_editorSubsystem->GetCurrentProject()->AddScene(scene);

    // Calculate memory pool usage
    Array<SizeType> memoryUsagePerPool;
    CalculateMemoryPoolUsage(memoryUsagePerPool);

    SizeType totalMemoryPoolUsage = 0;
    for (SizeType i = 0; i < memoryUsagePerPool.Size(); i++)
    {
        HYP_LOG(Editor, Debug, "Memory Usage for pool {} : {} MiB", i, double(memoryUsagePerPool[i]) / 1024 / 1024);
        totalMemoryPoolUsage += memoryUsagePerPool[i];
    }

    HYP_LOG(Editor, Debug, "Total Memory Usage for pools : {} MiB", double(totalMemoryPoolUsage) / 1024 / 1024);

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
        Vec3f(-0.4f, 0.8f, 0.0f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        5.0f);

    sunEntity->Attach(sunNode);
#endif

    // Add Skybox
    if (true)
    {
        Handle<Entity> skyboxEntity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent<TransformComponent>(skyboxEntity, TransformComponent { Transform(Vec3f::Zero(), Vec3f(1000.0f), Quaternion::Identity()) });

        scene->GetEntityManager()->AddComponent<SkyComponent>(skyboxEntity, SkyComponent {});
        scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(skyboxEntity, VisibilityStateComponent { VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE });
        scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(skyboxEntity, BoundingBoxComponent { BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)) });

        Handle<Node> skydomeNode = scene->GetRoot()->AddChild();
        skydomeNode->SetEntity(skyboxEntity);
        skydomeNode->SetName(NAME("Sky"));
    }

#if 1
    // Test assets, nevermind this
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    batch->Add("test_model", "models/testbed/testbed.obj");

    Handle<Entity> rootEntity = scene->GetEntityManager()->AddEntity();
    scene->GetRoot()->SetEntity(rootEntity);

    batch->OnComplete
        .Bind([this, scene](AssetMap& results)
            {
                Handle<Node> node = results["test_model"].ExtractAs<Node>();

                node->Scale(0.05f);
                node->SetName(NAME("test_model"));
                node->LockTransform();

                scene->GetRoot()->AddChild(node);

#if 1
                Handle<Node> envGridNode = scene->GetRoot()->AddChild();
                envGridNode->SetName(NAME("EnvGrid2"));

                Handle<Entity> envGridEntity = scene->GetEntityManager()->AddEntity<EnvGrid>(node->GetWorldAABB() * 1.01f, EnvGridOptions { .type = EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD, .density = Vec3u { 8, 4, 8 } });

                scene->GetEntityManager()->AddComponent<TransformComponent>(envGridEntity, TransformComponent {});
                scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(envGridEntity, BoundingBoxComponent { node->GetWorldAABB() * 1.01f, node->GetWorldAABB() * 1.01f });

                envGridNode->SetEntity(envGridEntity);
#endif

                if (auto& zombieAsset = results["zombie"]; zombieAsset.IsValid())
                {
                    Handle<Node> zombie = zombieAsset.ExtractAs<Node>();
                    zombie->Scale(0.25f);
                    zombie->Translate(Vec3f(0, 2.0f, -1.0f));

                    Handle<Entity> zombieEntity = zombie->GetChild(0)->GetEntity();

                    scene->GetRoot()->AddChild(zombie);

                    if (zombieEntity.IsValid())
                    {
                        if (auto* meshComponent = scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombieEntity))
                        {
                            meshComponent->material = meshComponent->material->Clone();
                            meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vec4f(1.0f));
                            meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.1f);
                            meshComponent->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                            InitObject(meshComponent->material);
                        }

                        scene->GetEntityManager()->AddComponent<AudioComponent>(zombieEntity, AudioComponent { .audioSource = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav")->Result(), .playbackState = { .loopMode = AudioLoopMode::AUDIO_LOOP_MODE_ONCE, .speed = 2.0f } });
                    }

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
        DebugDrawCommandList& debugDrawCommands = g_engine->GetDebugDrawer()->CreateCommandList();
        
//        PerformanceClock clock;
//        clock.Start();

        /*Proc<void(const VoxelOctree&, int)> drawOctant;

        drawOctant = [&](const VoxelOctree& octree, int depth)
        {
            if (octree.GetPayload().occupiedBit)
            {
                AssertDebug(!octree.IsDivided());
                debugDrawCommands.box(octree.GetAABB().GetCenter(), octree.GetAABB().GetExtent(), Color::Cyan());
            }
            
            if (octree.IsDivided())
            {
                for (const auto& it : octree.GetOctants())
                {
                    drawOctant(static_cast<const VoxelOctree&>(*it.octree), depth + 1);
                }
            }
        };

        drawOctant(*g_voxelOctree, 0);*/
//        
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
