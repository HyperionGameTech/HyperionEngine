/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <ArborEditor/ArborEditorExtension.hpp>
#include <ArborEditor/EditorArborState.hpp>
#include <ArborEditor/ArborShared.hpp>

#include <Editor/EditorState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/Prefab/EditorPrefabEditState.hpp>
#include <Editor/Instancing/EditorInstancePainterState.hpp>
#include <Editor/EditorProject.hpp>

#include <Asset/ModelLoaders/GLTFModelLoader.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Reflection/Property.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/World.hpp>

#include <Scene/Instancing/PrefabSet.hpp>
#include <Scene/Instancing/InstanceGroup.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Vertex.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

namespace Hyperion {
namespace tests {
namespace arbor {

namespace {

EditorSubsystem* GetEditorSubsystem()
{
    return EditorState::GetInstance()->GetEditorSubsystem().Get();
}

Handle<EditorArborState> GetArborState()
{
    Handle<ArborEditorExtension> extension = GetEditorSubsystem()->GetExtension<ArborEditorExtension>();

    return extension.IsValid() ? extension->GetArborState() : Handle<EditorArborState>::empty;
}

uint32 FindControl(const EditorArborState& state, const char* id)
{
    for (uint32 index = 0; index < state.GetControlCount(); index++)
    {
        if (state.GetControlId(index) == id)
        {
            return index;
        }
    }

    return ~0u;
}

} // namespace

HYP_EDITOR_TEST(ArborTools, ExtensionIsAttached)
{
    HYP_TEST_REQUIRE(GetArborState().IsValid());
    HYP_TEST_CHECK_MSG(ArborRecipeHandle::IsAvailable(), "{}", ArborRecipeHandle::GetLastError());
}

HYP_EDITOR_TEST(ArborTools, ContentTexturesDecodeForArbor)
{
    Vec2u extent;
    ByteBuffer albedo;
    HYP_TEST_REQUIRE_MSG(ReadArborMap("bark_oak_albedo.png", extent, albedo), "no Arbor_bark_oak_albedo; run the buildarbortextures commandlet");
    HYP_TEST_CHECK(extent.x > 0 && extent.y > 0 && extent.x <= 1024);
    HYP_TEST_CHECK(albedo.Size() == size_t(extent.x) * extent.y * 4);

    ByteBuffer roughness;
    HYP_TEST_REQUIRE(ReadArborMap("bark_oak_roughness.png", extent, roughness));

    bool grey = true;

    for (size_t index = 0; index + 3 < roughness.Size(); index += 4)
    {
        grey &= roughness.Data()[index] == roughness.Data()[index + 1] && roughness.Data()[index] == roughness.Data()[index + 2];
    }

    HYP_TEST_CHECK_MSG(grey, "roughness should come back grey, as Arbor reads it");

    ByteBuffer normal;
    HYP_TEST_REQUIRE(ReadArborMap("bark_oak_normal.png", extent, normal));

    uint64 sumZ = 0;

    for (size_t index = 2; index < normal.Size(); index += 4)
    {
        sumZ += normal.Data()[index];
    }

    // a tangent-space normal map points mostly out of the surface, so z is rebuilt well above the midpoint
    HYP_TEST_CHECK(sumZ / (normal.Size() / 4) > 200);

    HYP_TEST_CHECK(!ReadArborMap("no_such_map_albedo.png", extent, albedo));
}

HYP_EDITOR_TEST(ArborTools, EveryKindGrowsIntoAPrefab)
{
    HYP_TEST_REQUIRE(ArborRecipeHandle::IsAvailable());

    for (uint32 kind = 0; kind < uint32(ArborKind::Max); kind++)
    {
        HYP_TEST_REQUIRE(ArborRecipeHandle::GetPresetCount(kind) > 0);

        const String preset = ArborRecipeHandle::GetPresetName(kind, 0);
        ArborRecipeHandle recipe = ArborRecipeHandle::FromPreset(kind, preset);
        HYP_TEST_REQUIRE(recipe.IsValid());

        ByteBuffer glb;
        HYP_TEST_REQUIRE_MSG(recipe.GenerateGlb(glb), "{}: {}", preset, ArborRecipeHandle::GetLastError());

        AssetLoadResult result = GLTFModelLoader::LoadFromMemory(glb.ToByteView(), FilePath(HYP_FORMAT("{}.glb", preset)));
        HYP_TEST_REQUIRE_MSG(!result.HasError(), "{}: {}", preset, result.HasError() ? result.GetError().GetMessage() : "");

        Handle<Prefab> prefab = result.GetValue().ExtractAs<Prefab>();
        HYP_TEST_REQUIRE(prefab.IsValid() && prefab->GetRoot().IsValid());

        uint32 meshes = 0;
        bool swaysInWind = false;

        for (Node* descendant : prefab->GetRoot()->GetDescendants())
        {
            const Entity* entity = TryCast<Entity>(descendant);
            const MeshComponent* meshComponent = entity && entity->GetEntityManager() ? entity->TryGetComponent<MeshComponent>() : nullptr;

            if (meshComponent && meshComponent->mesh.IsValid())
            {
                meshes++;
                swaysInWind |= (meshComponent->mesh->GetMeshAttributes().inputLayout.mask & VT_Tree) != 0;
            }
        }

        HYP_TEST_CHECK_MSG(meshes > 0, "{} grew no meshes", preset);

        // rocks stand still; trees and cover carry ARBOR_tree_wind
        if (kind != uint32(ArborKind::Rock))
        {
            HYP_TEST_CHECK_MSG(swaysInWind, "{} has no tree wind vertices", preset);
        }
    }
}

HYP_EDITOR_TEST(ArborTools, GeneratedChildrenAreLeftOutOfSaves)
{
    Handle<Node> parent = MakeHandle<Node>();
    Handle<Node> kept = MakeHandle<Node>();
    Handle<Node> generated = MakeHandle<Node>();
    generated->SetNodeFlags(generated->GetNodeFlags() | NodeFlags::Generated);

    parent->AddChild(kept);
    parent->AddChild(generated);

    // the property the serializer reads
    const Property* children = Node::StaticClass()->GetProperty("Children"_sh);
    HYP_TEST_REQUIRE(children != nullptr && children->CanGet());

    BoxedValue saved = children->Get(BoxedValue(parent));
    HYP_TEST_REQUIRE(saved.Is<Node::NodeList>());

    HYP_TEST_CHECK(saved.Get<Node::NodeList>().Size() == 1);
    HYP_TEST_CHECK(saved.Get<Node::NodeList>()[0] == kept);
    HYP_TEST_CHECK(parent->GetChildren().Size() == 2);
}

// reads the recipe a plant's root carries, as the prefab keeps it
String ReadRecipeOf(const Node* plant)
{
    const String* ron = plant ? plant->GetDataEntry(NAME("ArborRecipe")).data.TryGet<String>() : nullptr;

    return ron ? *ron : String::empty;
}

HYP_EDITOR_TEST(ArborTools, ArborPrefabsOpenInArborMode)
{
    Handle<EditorArborState> state = GetArborState();
    HYP_TEST_REQUIRE(state.IsValid() && state->IsAvailable());

    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    ArborRecipeHandle recipe = ArborRecipeHandle::FromPreset(uint32(ArborKind::Rock), "stone");
    HYP_TEST_REQUIRE(recipe.IsValid());

    Handle<Entity> arborRoot = MakeHandle<Entity>(NAME("ArborStone"));
    arborRoot->AddDataEntry(NodeData(NAME("ArborKind"), int(ArborKind::Rock)));
    arborRoot->AddDataEntry(NodeData(NAME("ArborRecipe"), recipe.ToRon()));

    Handle<Prefab> arborPrefab = MakeHandle<Prefab>(NAME("ArborStone"), arborRoot);
    Handle<Prefab> plainPrefab = MakeHandle<Prefab>(NAME("PlainPrefab"), MakeHandle<Entity>(NAME("PlainPrefab")));

    // the later-registered Arbor mode is asked first; anything it does not claim still goes to prefab mode
    HYP_TEST_CHECK(editorSubsystem->FindEditStateForAsset(arborPrefab) == state.Get());
    HYP_TEST_CHECK(editorSubsystem->FindEditStateForAsset(plainPrefab) == editorSubsystem->GetPrefabEditState().Get());

    HYP_TEST_REQUIRE(editorSubsystem->EnterEditStateForAsset(arborPrefab));

    HYP_TEST_CHECK(state->IsActive());
    HYP_TEST_CHECK(state->HasTarget());
    HYP_TEST_CHECK(!state->IsNewPlant());
    HYP_TEST_CHECK(state->GetInspectedAsset().Get() == arborPrefab.Get());
    HYP_TEST_CHECK(editorSubsystem->GetActiveScene() == state->GetEditScene());

    HYP_TEST_CHECK(state->Exit(/* apply */ false));
    HYP_TEST_CHECK(!state->IsActive());
    HYP_TEST_CHECK(editorSubsystem->GetActiveScene() != state->GetEditScene());
}

HYP_EDITOR_TEST(ArborTools, NewPlantEditsUndoAndDiscard)
{
    Handle<EditorArborState> state = GetArborState();
    HYP_TEST_REQUIRE(state.IsValid() && state->IsAvailable());

    EditorSubsystem* editorSubsystem = GetEditorSubsystem();
    const Handle<Scene> previousScene = editorSubsystem->GetActiveScene();

    HYP_TEST_REQUIRE(state->EnterNewPlant(uint32(ArborKind::Tree), "hazel"));

    HYP_TEST_CHECK(state->IsActive());
    HYP_TEST_CHECK(state->IsNewPlant());
    HYP_TEST_CHECK(state->IsDirty());
    HYP_TEST_REQUIRE(state->GetEditScene().IsValid());

    HYP_TEST_CHECK(state->GetRecipePreset() == "hazel");

    state->ApplyPreset("oak_forest");
    HYP_TEST_CHECK(state->GetRecipePreset() == "oak_forest");

    state->ApplyPreset("hazel");
    HYP_TEST_CHECK(state->GetRecipePreset() == "hazel");

    const Node* plant = state->GetEditScene()->GetRoot().Get();
    const String original = ReadRecipeOf(plant);
    HYP_TEST_REQUIRE(original.Any());

    const uint32 length = FindControl(*state, "level_1.length_thickness.length");
    HYP_TEST_REQUIRE(length != ~0u);

    const float lo = state->GetControlLo(length);
    state->SetControlRanged(length, /* isRange */ true, lo * 0.5f, lo);
    state->CommitEdit();

    HYP_TEST_CHECK(state->IsControlRange(length));
    HYP_TEST_CHECK(ReadRecipeOf(plant) != original);

    // the mode's own undo stack
    editorSubsystem->GetSceneActionStack()->Undo();

    HYP_TEST_CHECK(ReadRecipeOf(plant) == original);
    HYP_TEST_CHECK(!state->IsControlRange(length));

    HYP_TEST_CHECK(state->Exit(/* apply */ false));
    HYP_TEST_CHECK(!state->IsActive());
    HYP_TEST_CHECK(editorSubsystem->GetActiveScene() == previousScene);
}

HYP_EDITOR_TEST(PrefabSet, ChooseOneFollowsWeights)
{
    Handle<Prefab> light = MakeHandle<Prefab>(NAME("PickLight"), MakeHandle<Entity>(NAME("PickLight")));
    Handle<Prefab> heavy = MakeHandle<Prefab>(NAME("PickHeavy"), MakeHandle<Entity>(NAME("PickHeavy")));
    Handle<Prefab> never = MakeHandle<Prefab>(NAME("PickNever"), MakeHandle<Entity>(NAME("PickNever")));

    Handle<PrefabSet> prefabSet = MakeHandle<PrefabSet>(NAME("PickSet"));

    HYP_TEST_CHECK(!prefabSet->ChooseOne(0.5f).IsValid());

    prefabSet->AddEntry(light, 1.0f);
    prefabSet->AddEntry(never, 0.0f);
    prefabSet->AddEntry(heavy, 3.0f);

    PrefabSetEntry missing;
    missing.weight = 10.0f;

    Array<PrefabSetEntry> entries = prefabSet->GetEntries();
    entries.PushBack(missing);
    prefabSet->SetEntries(entries);

    HYP_TEST_CHECK(prefabSet->GetValidEntries().Size() == 2);

    HYP_TEST_CHECK(prefabSet->ChooseOne(0.0f) == light);
    HYP_TEST_CHECK(prefabSet->ChooseOne(0.24f) == light);
    HYP_TEST_CHECK(prefabSet->ChooseOne(0.26f) == heavy);
    HYP_TEST_CHECK(prefabSet->ChooseOne(0.999f) == heavy);
    HYP_TEST_CHECK(prefabSet->ChooseOne(1.0f) == heavy);
}

HYP_EDITOR_TEST(ArborTools, BakingVariantsMakesASetToPaintWith)
{
    Handle<EditorArborState> state = GetArborState();
    HYP_TEST_REQUIRE(state.IsValid() && state->IsAvailable());

    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    HYP_TEST_REQUIRE(state->EnterNewPlant(uint32(ArborKind::Rock), "stone"));
    HYP_TEST_CHECK(state->CanSetVariantCount());

    state->SetVariantCount(3);
    HYP_TEST_CHECK(state->GetVariantCount() == 3);

    HYP_TEST_REQUIRE(state->GrowNow());

    HYP_TEST_REQUIRE(state->Apply());

    Handle<PrefabSet> prefabSet = TryCast<PrefabSet>(state->GetInspectedAsset());
    HYP_TEST_REQUIRE(prefabSet.IsValid());
    HYP_TEST_CHECK(prefabSet->IsRegistered());

    HYP_TEST_CHECK(state->GetLastBakedAsset().Get() == prefabSet.Get());
    HYP_TEST_CHECK(state->GetLastBakedAssetPath() == HYP_FORMAT("Misc/{}", prefabSet->GetName()));

    const Array<PrefabSetEntry> entries = prefabSet->GetValidEntries();
    HYP_TEST_REQUIRE(entries.Size() == 3);

    Array<String> recipes;

    for (const PrefabSetEntry& entry : entries)
    {
        HYP_TEST_CHECK(entry.prefab->IsRegistered());

        const String recipe = ReadRecipeOf(entry.prefab->GetRoot().Get());
        HYP_TEST_CHECK(recipe.Any());
        HYP_TEST_CHECK(!recipes.Contains(recipe));

        recipes.PushBack(recipe);
    }

    HYP_TEST_CHECK(state->Exit(/* apply */ false));

    HYP_TEST_CHECK(editorSubsystem->FindEditStateForAsset(prefabSet) == state.Get());

    HYP_TEST_REQUIRE(editorSubsystem->EnterEditStateForAsset(prefabSet));
    HYP_TEST_CHECK(state->GetVariantCount() == 3);
    HYP_TEST_CHECK(state->GetInspectedAsset().Get() == prefabSet.Get());
    HYP_TEST_CHECK(state->Exit(/* apply */ false));
}

HYP_EDITOR_TEST(ArborTools, RecipeRonAndControlsById)
{
    Handle<EditorArborState> state = GetArborState();
    HYP_TEST_REQUIRE(state.IsValid() && state->IsAvailable());

    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    HYP_TEST_CHECK(state->GetPlantKindCount() == 3);
    HYP_TEST_CHECK(state->GetPlantKindName(uint32(ArborKind::Rock)) == "Rock");
    HYP_TEST_CHECK(state->GetPlantKindName(99).Empty());

    HYP_TEST_REQUIRE(state->EnterNewPlant(uint32(ArborKind::Tree), "hazel"));

    // every control and its value in one call
    const String described = state->DescribeControls();
    const JSON::ParseResult parsed = JSON::Parse(described);
    HYP_TEST_REQUIRE_MSG(parsed.ok && parsed.value.IsObject(), "{}", parsed.message);

    const JSON::Object& description = parsed.value.AsObject();
    auto controlsIt = description.Find("controls");
    HYP_TEST_REQUIRE(controlsIt != description.End() && controlsIt->second.IsArray());

    const JSON::JArray& controls = controlsIt->second.AsArray();
    HYP_TEST_REQUIRE(controls.Size() == state->GetControlCount() && controls.Size() > 0);
    HYP_TEST_CHECK(controls[0].AsObject().Find("id")->second.AsString() == state->GetControlId(0));

    const String lengthId = "level_1.length_thickness.length";
    const int32 length = state->FindControl(lengthId);
    HYP_TEST_REQUIRE(length >= 0);
    HYP_TEST_CHECK(state->FindControl("no.such.control") == -1);

    const float lo = state->GetControlLo(uint32(length));
    HYP_TEST_REQUIRE(state->SetControlRangedById(lengthId, /* isRange */ true, lo * 0.5f, lo));
    HYP_TEST_CHECK(state->IsControlRange(uint32(length)));

    // a ranged control takes a range, not an integer
    HYP_TEST_CHECK(!state->SetControlIntegerById(lengthId, 3));

    // set by id is its own undoable edit
    editorSubsystem->GetSceneActionStack()->Undo();
    HYP_TEST_CHECK(!state->IsControlRange(uint32(length)));

    const String hazel = state->GetRecipeRon();
    HYP_TEST_REQUIRE(hazel.Any());

    const String oak = ArborRecipeHandle::FromPreset(uint32(ArborKind::Tree), "oak_forest").ToRon();
    HYP_TEST_REQUIRE(oak.Any());

    HYP_TEST_REQUIRE(state->SetRecipeRon(oak));
    HYP_TEST_CHECK(state->GetRecipePreset() == "oak_forest");

    HYP_TEST_CHECK(!state->SetRecipeRon("(not a recipe"));
    HYP_TEST_CHECK(state->GetStatusText().Any());
    HYP_TEST_CHECK(state->GetRecipePreset() == "oak_forest");

    editorSubsystem->GetSceneActionStack()->Undo();
    HYP_TEST_CHECK(state->GetRecipeRon() == hazel);

    HYP_TEST_CHECK(state->Exit(/* apply */ false));
}

HYP_EDITOR_TEST(InstancePainter, PaintAreaStampsWithoutAMouse)
{
    constexpr float GroundTop = 200.0f;

    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    const Handle<EditorProject>& project = editorSubsystem->GetCurrentProject();
    HYP_TEST_REQUIRE(project.IsValid() && project->GetWorld().IsValid());

    Handle<EditorInstancePainterState> painter = editorSubsystem->GetInstancePainterState();
    HYP_TEST_REQUIRE(painter.IsValid());

    // the vis thread leaves detached scenes alone, so the test can put its ground in this one's octree itself
    Handle<Scene> scene = MakeHandle<Scene>(NAME("PaintAreaScene"), SceneFlags::HAS_OCTREE | SceneFlags::DETACHED);
    InitObject(scene);
    project->GetWorld()->AddScene(scene, /* addToStreamingLayer */ false);

    // a 40 x 40 slab whose top is at y = 200, inside the octree's bounds and above whatever the project's own scenes have, which the
    // straight-down rays would otherwise find first
    Handle<Mesh> mesh = MeshBuilder::Cube();

    BVHNode bvh;

    {
        auto readScope = mesh->GetReadScope();
        mesh->BuildBVH(bvh);
    }

    {
        auto writeScope = mesh->GetWriteScope();
        mesh->SetBVH(std::move(bvh));
    }

    MaterialAttributes materialAttributes;
    materialAttributes.shaderName = NAME("GeometryPass");

    MeshComponent meshComponent;
    meshComponent.mesh = mesh;
    meshComponent.material = MakeHandle<Material>(NAME("PaintAreaGround"), materialAttributes);

    Handle<Entity> ground = MakeHandle<Entity>(NAME("PaintAreaGround"));
    ground->SetLocalTranslation(Vec3f(0.0f, GroundTop - 1.0f, 0.0f));
    ground->SetLocalScale(Vec3f(20.0f, 1.0f, 20.0f));

    // components need the scene's entity manager, so it goes in the scene first
    scene->GetRoot()->AddChild(ground);

    ground->AddComponent<MeshComponent>(meshComponent);
    ground->SetLocalBounds(mesh->GetAABB());

    HYP_TEST_REQUIRE(scene->GetOctree().Insert(ground.Get(), ground->GetWorldBounds()).HasValue());

    Handle<Prefab> prefab = MakeHandle<Prefab>(NAME("PaintAreaPrefab"), MakeHandle<Entity>(NAME("PaintAreaPrefab")));

    const float spacing = painter->GetSpacing();
    const float eraseRadius = painter->GetEraseRadius();

    painter->SetActivePrefab(prefab);
    painter->SetSpacing(0.5f);

    const uint32 placed = painter->PaintArea(Vec3f(0.0f, GroundTop, 0.0f), 10.0f, 20, /* erase */ false);

    HYP_TEST_CHECK(placed > 0 && placed <= 20);

    Handle<InstanceGroup> group = InstanceGroup::Find(scene.Get(), prefab);
    HYP_TEST_REQUIRE(group.IsValid());
    HYP_TEST_CHECK(group->NumInstances() == placed);

    painter->SetEraseRadius(100.0f);
    HYP_TEST_CHECK(painter->PaintArea(Vec3f(0.0f, GroundTop, 0.0f), 0.0f, 1, /* erase */ true) == 1);
    HYP_TEST_CHECK(group->NumInstances() == 0);

    // one undo entry per call
    project->GetActionStack()->Undo();
    HYP_TEST_CHECK(group->NumInstances() == placed);

    project->GetActionStack()->Undo();
    HYP_TEST_CHECK(!InstanceGroup::Find(scene.Get(), prefab).IsValid() || InstanceGroup::Find(scene.Get(), prefab)->NumInstances() == 0);

    painter->SetActivePrefab(Handle<Prefab>::Null());
    painter->SetSpacing(spacing);
    painter->SetEraseRadius(eraseRadius);

    project->GetWorld()->RemoveScene(scene.Get(), /* removeFromStreamingLayer */ false);
}

HYP_EDITOR_TEST(TerrainShaping, ShapesAndTakesPaint)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    const Handle<EditorProject>& project = editorSubsystem->GetCurrentProject();
    HYP_TEST_REQUIRE(project.IsValid() && project->GetWorld().IsValid());

    const Handle<WorldGrid>& worldGrid = project->GetWorld()->GetWorldGrid();
    HYP_TEST_REQUIRE(worldGrid.IsValid());

    Handle<TerrainWorldGridLayer> terrain = MakeHandle<TerrainWorldGridLayer>();
    terrain->SetName(NAME("ShapingTerrain"));

    TerrainLayerSettings settings = terrain->GetSettings();
    settings.heightSource = TerrainHeightSource::Flat;
    terrain->SetSettings(settings);

    worldGrid->AddLayer(terrain);

    const Vec2f at = { 1000.0f, 1000.0f };
    const float ground = terrain->GetHeightAt(at);

    auto isNear = [](float a, float b, float tolerance)
    {
        return MathUtil::Abs(a - b) <= tolerance;
    };

    terrain->FlattenRect(at - Vec2f(20.0f, 20.0f), at + Vec2f(20.0f, 20.0f), ground + 12.0f, 4.0f);

    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(at), ground + 12.0f, 0.01f));
    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(at + Vec2f(30.0f, 0.0f)), ground, 0.01f));

    const float inFalloff = terrain->GetHeightAt(at + Vec2f(22.0f, 0.0f));
    HYP_TEST_CHECK(inFalloff > ground + 0.1f && inFalloff < ground + 11.9f);

    terrain->FlattenCircle(Vec3f(at.x + 100.0f, ground + 5.0f, at.y), 5.0f, 3.0f);

    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(at + Vec2f(100.0f, 0.0f)), ground + 5.0f, 0.01f));
    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(at + Vec2f(110.0f, 0.0f)), ground, 0.01f));

    terrain->Ramp(Vec3f(at.x + 200.0f, ground, at.y), Vec3f(at.x + 240.0f, ground + 20.0f, at.y), 6.0f, 2.0f);

    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(at + Vec2f(220.0f, 0.0f)), ground + 10.0f, 0.6f));
    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(at + Vec2f(220.0f, 10.0f)), ground, 0.01f));

    // a hard step, smoothed: its top edge comes down toward the ground beside it
    const Vec2f step = at + Vec2f(0.0f, 300.0f);
    terrain->FlattenRect(step - Vec2f(10.0f, 10.0f), step + Vec2f(10.0f, 10.0f), ground + 8.0f, 0.0f);

    const Vec2f stepEdge = step + Vec2f(10.0f, 0.0f);
    HYP_TEST_CHECK(isNear(terrain->GetHeightAt(stepEdge), ground + 8.0f, 0.01f));

    terrain->SmoothArea(Vec3f(stepEdge.x, 0.0f, stepEdge.y), 4.0f, 1.0f);

    const float smoothed = terrain->GetHeightAt(stepEdge);
    HYP_TEST_CHECK(smoothed > ground && smoothed < ground + 7.9f);

    // the instance painter casts onto the terrain, and stamps land in the active scene rather than the terrain's own
    Handle<EditorInstancePainterState> painter = editorSubsystem->GetInstancePainterState();
    HYP_TEST_REQUIRE(painter.IsValid());

    Handle<Prefab> prefab = MakeHandle<Prefab>(NAME("TerrainPaintPrefab"), MakeHandle<Entity>(NAME("TerrainPaintPrefab")));

    const float spacing = painter->GetSpacing();
    painter->SetActivePrefab(prefab);
    painter->SetSpacing(1.0f);

    const uint32 placed = painter->PaintArea(Vec3f(at.x, 0.0f, at.y), 8.0f, 12, /* erase */ false);
    HYP_TEST_CHECK(placed > 0);

    Handle<InstanceGroup> group = InstanceGroup::Find(editorSubsystem->GetActiveScene().Get(), prefab);
    HYP_TEST_REQUIRE(group.IsValid());
    HYP_TEST_CHECK(group->NumInstances() == placed);

    Array<Transform, SceneAllocator> transforms;
    group->GetInstanceTransforms(transforms);

    bool allOnPlateau = !transforms.Empty();

    for (const Transform& transform : transforms)
    {
        allOnPlateau &= isNear(transform.GetTranslation().y, ground + 12.0f, 0.5f);
    }

    HYP_TEST_CHECK_MSG(allOnPlateau, "painted instances should stand on the flattened terrain at {}", ground + 12.0f);

    project->GetActionStack()->Undo();

    painter->SetActivePrefab(Handle<Prefab>::Null());
    painter->SetSpacing(spacing);

    worldGrid->RemoveLayer(terrain.Get());
}

} // namespace arbor
} // namespace tests
} // namespace Hyperion

#endif
