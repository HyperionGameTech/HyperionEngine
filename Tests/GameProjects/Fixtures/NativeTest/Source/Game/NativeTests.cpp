#include <Game/NativeTests.hpp>
#include <Game/NativeEngineTests.hpp>
#include <Game/NativeTestTypes.hpp>
#include <Game/NativeTestReloadTypes.hpp>
#include <Game/NativeTestMedkit.hpp>
#include <Game/NativeTestGame.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/EntityManager.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion::tests {

namespace {

static constexpr const char* g_weaponName = "NativeBlaster";
static constexpr const char* g_noteName = "NativeNote";

Handle<NativeTestWeaponAsset> CreateWeapon(const char* name)
{
    Handle<NativeTestWeaponAsset> weapon = MakeHandle<NativeTestWeaponAsset>();
    weapon->SetName(CreateNameFromDynamicString(name));
    weapon->damage = 42;
    weapon->fireRate = 2.5f;
    weapon->displayName = "Native Blaster";
    weapon->isAutomatic = true;
    weapon->muzzleOffset = Vec3f(1.0f, 2.0f, 3.0f);
    weapon->rarity = NativeTestRarity::Legendary;

    // a subclass of the field's type, which has to load back as itself
    Handle<NativeTestMedkit> ammo = MakeHandle<NativeTestMedkit>();
    ammo->pickupName = "Energy Cell";
    ammo->quantity = 30;
    ammo->healAmount = 12.5f;

    weapon->ammo = ammo;

    return weapon;
}

Handle<NativeTestNoteAsset> CreateNote(const char* name)
{
    Handle<NativeTestNoteAsset> note = MakeHandle<NativeTestNoteAsset>();
    note->SetName(CreateNameFromDynamicString(name));
    note->text = "Hello traveller";

    return note;
}

void CheckClass(GameTestContext& context, const Class* cls, const Class* expectedParent, const char* className)
{
    if (!context.Check(cls != nullptr, HYP_FORMAT("{} has no Class", className)))
    {
        return;
    }

    if (!context.Check(GetClass(StringHash(className)) == cls, HYP_FORMAT("{} is not in the class registry by name", className)))
    {
        return;
    }

    context.Check(cls->IsDerivedFrom(expectedParent), HYP_FORMAT("{} does not derive from {}", className, expectedParent->GetName()));
}

void CheckBucket(GameTestContext& context, const Handle<AssetObject>& asset, const AssetBucket& expectedBucket)
{
    context.GetAssetRegistry()->PutAsset(asset);

    if (!context.Check(asset->IsRegistered(), HYP_FORMAT("{} was not registered", asset->GetName())))
    {
        return;
    }

    context.Check(asset->GetPath().GetBucket() == expectedBucket,
        HYP_FORMAT("{} went in bucket {}, expected {}", asset->GetName(), asset->GetPath().GetBucket().GetName(), expectedBucket.GetName()));
}

void SaveAsset(GameTestContext& context, const Handle<AssetObject>& asset, const AssetBucket& expectedBucket)
{
    // a project made in the editor starts out with its bucket folders; this one starts with none
    const FilePath bucketDirectory = context.GetContentDirectory() / expectedBucket.GetName();

    if (!bucketDirectory.Exists())
    {
        bucketDirectory.MkDir();
    }

    CheckBucket(context, asset, expectedBucket);

    if (context.HasFailed())
    {
        return;
    }

    if (Result saveResult = asset->Save(); !context.Check(!saveResult.HasError(), HYP_FORMAT("Saving {} failed: {}", asset->GetName(), saveResult.HasError() ? saveResult.GetError().GetMessage() : String::empty)))
    {
        return;
    }

    const FilePath manifestPath = bucketDirectory / (asset->GetName().ToString() + ".hmf");

    context.Check(manifestPath.Exists(), HYP_FORMAT("{} was not written", manifestPath));
}

#pragma region Write phase

void TypesAreRegistered(GameTestContext& context)
{
    CheckClass(context, NativeTestPickup::StaticClass(), ObjectBase::StaticClass(), "NativeTestPickup");
    CheckClass(context, NativeTestWeaponAsset::StaticClass(), AssetObject::StaticClass(), "NativeTestWeaponAsset");
    CheckClass(context, NativeTestNoteAsset::StaticClass(), AssetObject::StaticClass(), "NativeTestNoteAsset");
    CheckClass(context, NativeTestMedkit::StaticClass(), NativeTestPickup::StaticClass(), "NativeTestMedkit");
    CheckClass(context, NativeTestGame::StaticClass(), Game::StaticClass(), "NativeTestGame");
}

void CodeGen_ReflectsMembers(GameTestContext& context)
{
    const Class* weaponClass = NativeTestWeaponAsset::StaticClass();

    for (const char* fieldName : { "Damage", "FireRate", "DisplayName", "IsAutomatic", "MuzzleOffset", "Rarity", "Ammo" })
    {
        context.Check(weaponClass->GetField(StringHash(fieldName)) != nullptr, HYP_FORMAT("NativeTestWeaponAsset has no field {}", fieldName));
    }

    context.Check(weaponClass->GetAttribute("assetbucket"_sh).IsString(), "NativeTestWeaponAsset has no assetbucket attribute");

    context.Check(NativeTestPickup::StaticClass()->GetMethod(StringHash("GetDoubledQuantity")) != nullptr, "NativeTestPickup has no GetDoubledQuantity method");
}

void CodeGen_DerivedGameClassInheritsMembers(GameTestContext& context)
{
    const Class* medkitClass = NativeTestMedkit::StaticClass();

    context.Check(medkitClass->GetField(StringHash("HealAmount")) != nullptr, "NativeTestMedkit has no HealAmount field");
    context.Check(medkitClass->GetField(StringHash("PickupName"), /* deep */ true) != nullptr, "NativeTestMedkit doesn't inherit PickupName");
    context.Check(medkitClass->GetMethod(StringHash("GetDoubledQuantity"), /* deep */ true) != nullptr, "NativeTestMedkit doesn't inherit GetDoubledQuantity");
}

void CodeGen_ReflectsEnum(GameTestContext& context)
{
    const Class* enumClass = GetClass(StringHash("NativeTestRarity"));

    if (!context.Check(enumClass != nullptr, "NativeTestRarity is not in the class registry"))
    {
        return;
    }

    context.Check(enumClass->IsEnumType(), "NativeTestRarity's class is not an enum class");
}

void WeaponAsset_GoesInAttributeBucket(GameTestContext& context)
{
    CheckBucket(context, CreateWeapon("BucketProbeWeapon"), AssetBuckets::RawData);
}

void NoteAsset_GoesInMiscBucket(GameTestContext& context)
{
    CheckBucket(context, CreateNote("BucketProbeNote"), AssetBuckets::Misc);
}

void WeaponAsset_SavesManifest(GameTestContext& context)
{
    SaveAsset(context, CreateWeapon(g_weaponName), AssetBuckets::RawData);
}

void NoteAsset_SavesManifest(GameTestContext& context)
{
    SaveAsset(context, CreateNote(g_noteName), AssetBuckets::Misc);
}

#pragma endregion Write phase

#pragma region Read phase

void WeaponAsset_LoadsSavedFields(GameTestContext& context)
{
    Handle<AssetObject> asset = context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash(g_weaponName));

    if (!context.Check(asset.IsValid(), HYP_FORMAT("RawData/{} did not load", g_weaponName)))
    {
        return;
    }

    if (!context.Check(asset->IsA<NativeTestWeaponAsset>(), HYP_FORMAT("RawData/{} loaded as {}", g_weaponName, asset->InstanceClass()->GetName())))
    {
        return;
    }

    const NativeTestWeaponAsset& weapon = *static_cast<NativeTestWeaponAsset*>(asset.Get());

    context.Check(weapon.damage == 42, HYP_FORMAT("damage: expected 42, got {}", weapon.damage));
    context.Check(weapon.fireRate == 2.5f, HYP_FORMAT("fireRate: expected 2.5, got {}", weapon.fireRate));
    context.Check(weapon.displayName == "Native Blaster", HYP_FORMAT("displayName: expected 'Native Blaster', got '{}'", weapon.displayName));
    context.Check(weapon.isAutomatic, "isAutomatic: expected true");
    context.Check(weapon.muzzleOffset == Vec3f(1.0f, 2.0f, 3.0f), HYP_FORMAT("muzzleOffset: expected (1, 2, 3), got {}", weapon.muzzleOffset));
    context.Check(weapon.rarity == NativeTestRarity::Legendary, HYP_FORMAT("rarity: expected Legendary (3), got {}", uint32(weapon.rarity)));

    if (!context.Check(weapon.ammo.IsValid(), "ammo did not load"))
    {
        return;
    }

    context.Check(weapon.ammo->pickupName == "Energy Cell", HYP_FORMAT("ammo.pickupName: expected 'Energy Cell', got '{}'", weapon.ammo->pickupName));
    context.Check(weapon.ammo->quantity == 30, HYP_FORMAT("ammo.quantity: expected 30, got {}", weapon.ammo->quantity));

    if (!context.Check(weapon.ammo->IsA<NativeTestMedkit>(), HYP_FORMAT("ammo loaded as {}, expected NativeTestMedkit", weapon.ammo->InstanceClass()->GetName())))
    {
        return;
    }

    const NativeTestMedkit& medkit = *static_cast<NativeTestMedkit*>(weapon.ammo.Get());

    context.Check(medkit.healAmount == 12.5f, HYP_FORMAT("ammo.healAmount: expected 12.5, got {}", medkit.healAmount));
}

void NoteAsset_LoadsSavedFields(GameTestContext& context)
{
    Handle<AssetObject> asset = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash(g_noteName));

    if (!context.Check(asset.IsValid(), HYP_FORMAT("Misc/{} did not load", g_noteName)))
    {
        return;
    }

    if (!context.Check(asset->IsA<NativeTestNoteAsset>(), HYP_FORMAT("Misc/{} loaded as {}", g_noteName, asset->InstanceClass()->GetName())))
    {
        return;
    }

    const NativeTestNoteAsset& note = *static_cast<NativeTestNoteAsset*>(asset.Get());

    context.Check(note.text == "Hello traveller", HYP_FORMAT("text: expected 'Hello traveller', got '{}'", note.text));
}

#pragma endregion Read phase

#pragma region Cooked phase

// what NativeTestEditorTests' cook-author phase saved in the editor, as the packaged game loads it from the cook

Scene* FindCookScene(GameTestContext& context)
{
    const Handle<World>& world = context.GetGame()->GetWorld();

    if (!context.Check(world.IsValid() && !world->IsTransient(), "the game didn't load a World"))
    {
        return nullptr;
    }

    if (!context.Check(world->GetName() == NAME("MainWorld"), HYP_FORMAT("the game loaded World {}, expected MainWorld", world->GetName())))
    {
        return nullptr;
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (scene.IsValid() && scene->GetName() == NAME("CookScene") && scene->GetRoot().IsValid())
        {
            return scene.Get();
        }
    }

    context.Check(false, "MainWorld has no CookScene");

    return nullptr;
}

NativeTestReloadEntity* FindHero(GameTestContext& context)
{
    Scene* scene = FindCookScene(context);
    Node* node = scene != nullptr ? scene->GetRoot()->FindChildByName(StringHash("Hero")) : nullptr;

    if (!context.Check(scene == nullptr || node != nullptr, "CookScene has no Hero"))
    {
        return nullptr;
    }

    if (node != nullptr && !context.Check(node->IsA<NativeTestReloadEntity>(), HYP_FORMAT("Hero loaded as {}, expected NativeTestReloadEntity", node->InstanceClass()->GetName())))
    {
        return nullptr;
    }

    return static_cast<NativeTestReloadEntity*>(node);
}

void Cooked_LoadsStartupWorld(GameTestContext& context)
{
    FindCookScene(context);
}

void Cooked_LoadsGameEntityFields(GameTestContext& context)
{
    if (NativeTestReloadEntity* hero = FindHero(context))
    {
        context.Check(hero->health == 64, HYP_FORMAT("Hero.Health: expected 64, got {}", hero->health));
        context.Check(hero->label == "cooked", HYP_FORMAT("Hero.Label: expected 'cooked', got '{}'", hero->label));
    }
}

void Cooked_LoadsGameComponent(GameTestContext& context)
{
    NativeTestReloadEntity* hero = FindHero(context);
    const NativeTestReloadComponent* component = hero != nullptr ? hero->TryGetComponent<NativeTestReloadComponent>() : nullptr;

    if (!hero || !context.Check(component != nullptr, "Hero has no NativeTestReloadComponent"))
    {
        return;
    }

    context.Check(component->speed == 6.5f, HYP_FORMAT("Speed: expected 6.5, got {}", component->speed));
    context.Check(component->tag == "cooked", HYP_FORMAT("Tag: expected 'cooked', got '{}'", component->tag));
}

void Cooked_LoadsReferencedGameAsset(GameTestContext& context)
{
    NativeTestReloadEntity* hero = FindHero(context);

    if (!hero || !context.Check(hero->note.IsValid(), "Hero.Note didn't load"))
    {
        return;
    }

    context.Check(hero->note->text == "packed", HYP_FORMAT("Note.Text: expected 'packed', got '{}'", hero->note->text));
    context.Check(hero->note->priority == 9, HYP_FORMAT("Note.Priority: expected 9, got {}", hero->note->priority));
}

#pragma endregion Cooked phase

} // namespace

const Array<GameTestDefinition>& GetNativeGameTests()
{
    static const Array<GameTestDefinition> s_tests = []()
    {
        Array<GameTestDefinition> tests {
            { "TypesAreRegistered", g_writePhase, &TypesAreRegistered },
            { "CodeGen_ReflectsMembers", g_writePhase, &CodeGen_ReflectsMembers },
            { "CodeGen_DerivedGameClassInheritsMembers", g_writePhase, &CodeGen_DerivedGameClassInheritsMembers },
            { "CodeGen_ReflectsEnum", g_writePhase, &CodeGen_ReflectsEnum },
            { "WeaponAsset_GoesInAttributeBucket", g_writePhase, &WeaponAsset_GoesInAttributeBucket },
            { "NoteAsset_GoesInMiscBucket", g_writePhase, &NoteAsset_GoesInMiscBucket },
            { "WeaponAsset_SavesManifest", g_writePhase, &WeaponAsset_SavesManifest },
            { "NoteAsset_SavesManifest", g_writePhase, &NoteAsset_SavesManifest },
            { "WeaponAsset_LoadsSavedFields", g_readPhase, &WeaponAsset_LoadsSavedFields },
            { "NoteAsset_LoadsSavedFields", g_readPhase, &NoteAsset_LoadsSavedFields },
            { "Cooked_LoadsStartupWorld", g_cookedPhase, &Cooked_LoadsStartupWorld },
            { "Cooked_LoadsGameEntityFields", g_cookedPhase, &Cooked_LoadsGameEntityFields },
            { "Cooked_LoadsGameComponent", g_cookedPhase, &Cooked_LoadsGameComponent },
            { "Cooked_LoadsReferencedGameAsset", g_cookedPhase, &Cooked_LoadsReferencedGameAsset }
        };

        tests.Concat(GetNativeEngineTests());

        return tests;
    }();

    return s_tests;
}

} // namespace Hyperion::tests
