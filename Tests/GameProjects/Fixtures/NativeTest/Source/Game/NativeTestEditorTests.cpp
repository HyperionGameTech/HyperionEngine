#include <Game/NativeTestEditorTests.hpp>
#include <Game/NativeTestReloadTypes.hpp>
#include <Game/NativeTestBuild.hpp>
#include <Game/GameTestRunner.hpp>

#include <Framework/Game.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/Format.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <NativeTestEditorTests.generated.inl>

namespace Hyperion {

namespace {

using tests::GameTestContext;
using tests::GameTestDefinition;

// reload: setup runs on the first build, verify on the second (Fixtures/NativeTestReload/V2), release once its checks are done.
// cook-author builds what the cook tests cook
constexpr const char* SetupPhase = "reload-setup";
constexpr const char* VerifyPhase = "reload-verify";
constexpr const char* ReleasePhase = "reload-release";
constexpr const char* CookAuthorPhase = "cook-author";

constexpr UUID EntityAUuid { 0x4e54526c6f616441ull, 0x1ull };
constexpr UUID EntityBUuid { 0x4e54526c6f616442ull, 0x2ull };

Scene* FindReloadScene(GameTestContext& context)
{
    if (!context.Check(context.GetWorld().IsValid(), "there's no World being edited"))
    {
        return nullptr;
    }

    for (const Handle<Scene>& scene : context.GetWorld()->GetScenes())
    {
        if (scene.IsValid() && scene->GetName() == NAME("NativeTestReloadScene") && scene->GetRoot().IsValid())
        {
            return scene.Get();
        }
    }

    context.Check(false, "NativeTestReloadScene is gone from the World");

    return nullptr;
}

Node* FindNode(GameTestContext& context, Scene* scene, const char* name)
{
    Node* node = scene->GetRoot()->FindChildByName(StringHash(name));

    context.Check(node != nullptr, HYP_FORMAT("{} is gone from the scene", name));

    return node;
}

void AddReloadComponent(Entity* entity, float speed, const char* tag)
{
    NativeTestReloadComponent component;
    component.speed = speed;
    component.tag = tag;

    entity->AddComponent<NativeTestReloadComponent>(std::move(component));
}

#pragma region Setup phase

// the first build only: the second one doesn't have NativeTestObsoleteEntity
#if NATIVE_TEST_BUILD < 2

void Setup_BuildsObjectsOfGameClasses(GameTestContext& context)
{
    const Handle<World>& world = context.GetWorld();

    if (!context.Check(world.IsValid(), "there's no World being edited"))
    {
        return;
    }

    Handle<Scene> scene = MakeHandle<Scene>(NAME("NativeTestReloadScene"));
    scene->SetSceneFlags(SceneFlags::DEFAULT & ~SceneFlags::STREAMED);

    world->AddScene(scene, /* addToStreamingLayer */ true);

    const Handle<Node>& root = scene->GetRoot();

    Handle<Entity> before = MakeHandle<Entity>();
    before->SetName(NAME("Before"));
    root->AddChild(before);

    Handle<NativeTestReloadNote> note = MakeHandle<NativeTestReloadNote>();
    note->SetName(NAME("ReloadNote"));
    note->text = "remember";
    note->priority = 3;

    context.GetAssetRegistry()->PutAsset(note);

    Handle<NativeTestReloadEntity> entityA = MakeHandle<NativeTestReloadEntity>();
    entityA->SetName(NAME("A"));
    entityA->SetUUID(EntityAUuid);
    entityA->health = 42;
    entityA->label = "alpha";
    entityA->buddy = before;
    entityA->note = note;
    root->AddChild(entityA);

    AddReloadComponent(entityA, 3.0f, "on A");

    Handle<Node> childOfA = MakeHandle<Node>();
    childOfA->SetName(NAME("ChildOfA"));
    entityA->AddChild(childOfA);

    Handle<NativeTestReloadEntity> entityB = MakeHandle<NativeTestReloadEntity>();
    entityB->SetName(NAME("B"));
    entityB->SetUUID(EntityBUuid);
    entityB->health = 7;
    entityB->label = "beta";
    root->AddChild(entityB);

    // refer to each other, so each is rebuilt while the other still refers to its previous build
    entityA->partner = entityB;
    entityB->partner = entityA;

    Handle<NativeTestObsoleteEntity> obsolete = MakeHandle<NativeTestObsoleteEntity>();
    obsolete->SetName(NAME("Obsolete"));
    root->AddChild(obsolete);

    Handle<Entity> holder = MakeHandle<Entity>();
    holder->SetName(NAME("Holder"));
    root->AddChild(holder);

    AddReloadComponent(holder, 4.5f, "fast");

    Handle<Node> after = MakeHandle<Node>();
    after->SetName(NAME("After"));
    root->AddChild(after);

    Handle<NativeTestReloadSystem> system = MakeHandle<NativeTestReloadSystem>();
    system->counter = 11;
    world->AddSystem(system);

    world->AddSubsystem<NativeTestReloadSubsystem>()->ticks = 5;

    context.Check(root->GetChildren().Size() == 6, HYP_FORMAT("the scene should have 6 nodes, has {}", root->GetChildren().Size()));
    context.Check(note->IsRegistered(), "ReloadNote was not registered");
    context.Check(world->GetSystem<NativeTestReloadSystem>() == system.Get(), "NativeTestReloadSystem was not added to the World");
    context.Check(world->GetSubsystem<NativeTestReloadSubsystem>() != nullptr, "NativeTestReloadSubsystem was not added to the World");
}

#endif // NATIVE_TEST_BUILD < 2

#pragma endregion Setup phase

#pragma region Verify phase

#if NATIVE_TEST_BUILD >= 2

NativeTestReloadEntity* FindCurrentEntity(GameTestContext& context, Scene* scene, const UUID& uuid, const char* name)
{
    Node* node = scene->GetRoot()->FindChildByUUID(uuid);

    if (!context.Check(node != nullptr, HYP_FORMAT("no node in the scene has {}'s UUID", name)))
    {
        return nullptr;
    }

    if (!context.Check(node->InstanceClass() == NativeTestReloadEntity::StaticClass(),
            HYP_FORMAT("{} is a {} of the previous build", name, node->InstanceClass()->GetName())))
    {
        return nullptr;
    }

    return static_cast<NativeTestReloadEntity*>(node);
}

void Verify_EntitiesAreOnNewBuild(GameTestContext& context)
{
    if (Scene* scene = FindReloadScene(context))
    {
        FindCurrentEntity(context, scene, EntityAUuid, "A");
        FindCurrentEntity(context, scene, EntityBUuid, "B");
    }
}

void Verify_KeepsSiblingOrderAndChildren(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);

    if (!scene)
    {
        return;
    }

    const char* expectedOrder[] = { "Before", "A", "B", "Obsolete", "Holder", "After" };
    const auto& children = scene->GetRoot()->GetChildren();

    if (!context.Check(children.Size() == std::size(expectedOrder), HYP_FORMAT("the scene should have {} nodes, has {}", std::size(expectedOrder), children.Size())))
    {
        return;
    }

    for (uint32 index = 0; index < std::size(expectedOrder); index++)
    {
        context.Check(children[index]->GetName() == StringHash(expectedOrder[index]),
            HYP_FORMAT("node {}: expected {}, got {}", index, expectedOrder[index], children[index]->GetName()));
    }

    if (NativeTestReloadEntity* entityA = FindCurrentEntity(context, scene, EntityAUuid, "A"))
    {
        context.Check(entityA->GetChildren().Size() == 1 && entityA->GetChildren()[0]->GetName() == NAME("ChildOfA"), "A's child didn't move to its new build");
    }
}

void Verify_CarriesFieldsOver(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);

    if (!scene)
    {
        return;
    }

    if (NativeTestReloadEntity* entityA = FindCurrentEntity(context, scene, EntityAUuid, "A"))
    {
        // an int32 in the first build, a double in this one
        context.Check(entityA->health == 42.0, HYP_FORMAT("A.Health: expected 42, got {}", entityA->health));
        context.Check(entityA->label == "alpha", HYP_FORMAT("A.Label: expected 'alpha', got '{}'", entityA->label));
        context.Check(entityA->armor == 5, HYP_FORMAT("A.Armor is new, so it should have its default 5, got {}", entityA->armor));
    }

    if (NativeTestReloadEntity* entityB = FindCurrentEntity(context, scene, EntityBUuid, "B"))
    {
        context.Check(entityB->health == 7.0, HYP_FORMAT("B.Health: expected 7, got {}", entityB->health));
        context.Check(entityB->label == "beta", HYP_FORMAT("B.Label: expected 'beta', got '{}'", entityB->label));
    }
}

void Verify_RetargetsReferences(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);

    if (!scene)
    {
        return;
    }

    NativeTestReloadEntity* entityA = FindCurrentEntity(context, scene, EntityAUuid, "A");
    NativeTestReloadEntity* entityB = FindCurrentEntity(context, scene, EntityBUuid, "B");
    Node* before = FindNode(context, scene, "Before");

    if (!entityA || !entityB || !before)
    {
        return;
    }

    context.Check(entityA->buddy.Get() == before, "A.Buddy doesn't refer to Before");
    context.Check(entityA->partner.Get() == entityB, "A.Partner doesn't refer to B's new build");
    context.Check(entityB->partner.Get() == entityA, "B.Partner doesn't refer to A's new build");
}

void Verify_RebuildsAssets(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);
    NativeTestReloadEntity* entityA = scene != nullptr ? FindCurrentEntity(context, scene, EntityAUuid, "A") : nullptr;

    if (!entityA || !context.Check(entityA->note.IsValid(), "A.Note was cleared"))
    {
        return;
    }

    const NativeTestReloadNote& note = *entityA->note;

    if (!context.Check(note.InstanceClass() == NativeTestReloadNote::StaticClass(), "A.Note is still on the previous build"))
    {
        return;
    }

    context.Check(note.text == "remember", HYP_FORMAT("Note.Text: expected 'remember', got '{}'", note.text));
    context.Check(note.priority == 3, HYP_FORMAT("Note.Priority: expected 3, got {}", note.priority));
    context.Check(note.mood == "calm", HYP_FORMAT("Note.Mood is new, so it should have its default 'calm', got '{}'", note.mood));
    context.Check(note.IsRegistered(), "the rebuilt note isn't registered");

    Handle<AssetObject> registered = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("ReloadNote"));

    context.Check(registered.Get() == entityA->note.Get(), "the registry hands out another object than A.Note for Misc/ReloadNote");
}

void Verify_MigratesComponents(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);

    if (!scene)
    {
        return;
    }

    struct ExpectedComponent
    {
        Entity* entity;
        const char* name;
        float speed;
        const char* tag;
    };

    Entity* holder = TryCast<Entity>(FindNode(context, scene, "Holder"));

    const ExpectedComponent expectedComponents[] = {
        { holder, "Holder", 4.5f, "fast" },
        { FindCurrentEntity(context, scene, EntityAUuid, "A"), "A", 3.0f, "on A" }
    };

    for (const ExpectedComponent& expected : expectedComponents)
    {
        if (!expected.entity)
        {
            continue;
        }

        const NativeTestReloadComponent* component = expected.entity->TryGetComponent<NativeTestReloadComponent>();

        if (!context.Check(component != nullptr, HYP_FORMAT("{} lost its NativeTestReloadComponent", expected.name)))
        {
            continue;
        }

        context.Check(component->speed == expected.speed, HYP_FORMAT("{}'s Speed: expected {}, got {}", expected.name, expected.speed, component->speed));
        context.Check(component->tag == expected.tag, HYP_FORMAT("{}'s Tag: expected '{}', got '{}'", expected.name, expected.tag, component->tag));
        context.Check(component->boost == 2.0f, HYP_FORMAT("{}'s Boost is new, so it should have its default 2, got {}", expected.name, component->boost));
    }
}

void Verify_ReplacesSystems(GameTestContext& context)
{
    const Handle<World>& world = context.GetWorld();

    if (!context.Check(world.IsValid(), "there's no World being edited"))
    {
        return;
    }

    uint32 numSystems = 0;

    for (const Handle<SystemBase>& system : world->GetSystems())
    {
        if (!system.IsValid() || system->InstanceClass()->GetName() != NAME("NativeTestReloadSystem"))
        {
            continue;
        }

        numSystems++;

        if (context.Check(system->InstanceClass() == NativeTestReloadSystem::StaticClass(), "NativeTestReloadSystem is still on the previous build"))
        {
            const int32 counter = static_cast<const NativeTestReloadSystem&>(*system).counter;

            context.Check(counter == 11, HYP_FORMAT("NativeTestReloadSystem.Counter: expected 11, got {}", counter));
        }
    }

    context.Check(numSystems == 1, HYP_FORMAT("the World should have one NativeTestReloadSystem, has {}", numSystems));

    const Subsystem* subsystem = world->GetSubsystem(TypeId::ForType<NativeTestReloadSubsystem>());

    if (!context.Check(subsystem != nullptr, "NativeTestReloadSubsystem is gone from the World"))
    {
        return;
    }

    if (context.Check(subsystem->InstanceClass() == NativeTestReloadSubsystem::StaticClass(), "NativeTestReloadSubsystem is still on the previous build"))
    {
        const int32 ticks = static_cast<const NativeTestReloadSubsystem*>(subsystem)->ticks;

        context.Check(ticks == 5, HYP_FORMAT("NativeTestReloadSubsystem.Ticks: expected 5, got {}", ticks));
    }
}

void Verify_KeepsObjectsOfRemovedClasses(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);
    Node* obsolete = scene != nullptr ? FindNode(context, scene, "Obsolete") : nullptr;

    if (!obsolete)
    {
        return;
    }

    context.Check(obsolete->InstanceClass()->GetName() == NAME("NativeTestObsoleteEntity"),
        HYP_FORMAT("Obsolete should still be a NativeTestObsoleteEntity, is a {}", obsolete->InstanceClass()->GetName()));

    context.Check(GetClass(StringHash("NativeTestObsoleteEntity")) == nullptr, "NativeTestObsoleteEntity is still registered, but this build doesn't have it");
}

#endif // NATIVE_TEST_BUILD >= 2

#pragma endregion Verify phase

#pragma region Release phase

// everything the first build made, engine objects included (their vtables are its copies), so the commandlet can wait for it to unload
void Release_RemovesWhatTheFirstBuildMade(GameTestContext& context)
{
    Scene* scene = FindReloadScene(context);

    if (!scene)
    {
        return;
    }

    for (const UUID& uuid : { EntityAUuid, EntityBUuid })
    {
        if (NativeTestReloadEntity* entity = TryCast<NativeTestReloadEntity>(scene->GetRoot()->FindChildByUUID(uuid)))
        {
            entity->partner.Reset();
        }
    }

    context.GetWorld()->RemoveScene(scene, /* removeFromStreamingLayer */ true);

    auto isReloadScene = [scene](AssetObject* asset)
    {
        return asset == scene;
    };

    context.GetAssetRegistry()->RemoveCachedIf(AssetBuckets::Scenes, isReloadScene);

    for (const Handle<Scene>& remainingScene : context.GetWorld()->GetScenes())
    {
        context.Check(remainingScene.Get() != scene, "NativeTestReloadScene is still in the World");
    }
}

#pragma endregion Release phase

#pragma region Cook author phase

// what the cook tests cook, saved by the editor with the project; the cooked game reads it back in its cooked phase (NativeTests.cpp)
void CookAuthor_BuildsWorldContent(GameTestContext& context)
{
    const Handle<World>& world = context.GetWorld();

    if (!context.Check(world.IsValid(), "there's no World being edited"))
    {
        return;
    }

    Handle<Scene> scene = MakeHandle<Scene>(NAME("CookScene"));
    scene->SetSceneFlags(SceneFlags::DEFAULT & ~SceneFlags::STREAMED);

    world->AddScene(scene, /* addToStreamingLayer */ true);
    world->MarkDirty();

    Handle<NativeTestReloadNote> note = MakeHandle<NativeTestReloadNote>();
    note->SetName(NAME("CookNote"));
    note->text = "packed";
    note->priority = 9;

    context.GetAssetRegistry()->PutAsset(note);

    Handle<NativeTestReloadEntity> hero = MakeHandle<NativeTestReloadEntity>();
    hero->SetName(NAME("Hero"));
    hero->health = 64;
    hero->label = "cooked";
    hero->note = note;
    scene->GetRoot()->AddChild(hero);

    AddReloadComponent(hero, 6.5f, "cooked");

    context.Check(note->IsRegistered(), "CookNote was not registered");
}

#pragma endregion Cook author phase

const Array<GameTestDefinition>& GetEditorTests()
{
    static const Array<GameTestDefinition> s_tests {
        { "CookAuthor_BuildsWorldContent", CookAuthorPhase, &CookAuthor_BuildsWorldContent },
#if NATIVE_TEST_BUILD < 2
        { "Setup_BuildsObjectsOfGameClasses", SetupPhase, &Setup_BuildsObjectsOfGameClasses },
#endif
#if NATIVE_TEST_BUILD >= 2
        { "Verify_EntitiesAreOnNewBuild", VerifyPhase, &Verify_EntitiesAreOnNewBuild },
        { "Verify_KeepsSiblingOrderAndChildren", VerifyPhase, &Verify_KeepsSiblingOrderAndChildren },
        { "Verify_CarriesFieldsOver", VerifyPhase, &Verify_CarriesFieldsOver },
        { "Verify_RetargetsReferences", VerifyPhase, &Verify_RetargetsReferences },
        { "Verify_RebuildsAssets", VerifyPhase, &Verify_RebuildsAssets },
        { "Verify_MigratesComponents", VerifyPhase, &Verify_MigratesComponents },
        { "Verify_ReplacesSystems", VerifyPhase, &Verify_ReplacesSystems },
        { "Verify_KeepsObjectsOfRemovedClasses", VerifyPhase, &Verify_KeepsObjectsOfRemovedClasses },
#endif
        { "Release_RemovesWhatTheFirstBuildMade", ReleasePhase, &Release_RemovesWhatTheFirstBuildMade }
    };

    return s_tests;
}

} // namespace

String NativeTestEditorTests::RunPhase(const String& phase, const Handle<Game>& game, const Handle<World>& world)
{
    const Handle<AssetRegistry> assetRegistry = game.IsValid() ? game->GetAssetRegistry() : Handle<AssetRegistry>();

    tests::GameTestContext context(game.Get(), assetRegistry, phase, world);

    return tests::RunGameTests(context, GetEditorTests());
}

} // namespace Hyperion
