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

#include <Asset/ModelLoaders/GLTFModelLoader.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Reflection/Property.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Instancing/PrefabSet.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Vertex.hpp>

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

} // namespace arbor
} // namespace tests
} // namespace Hyperion

#endif
