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

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/Format.hpp>

#include <cstring>

namespace Hyperion::tests {

namespace {

static constexpr const char* g_weaponName = "NativeBlaster";
static constexpr const char* g_noteName = "NativeNote";
static constexpr const char* g_armorName = "NativeArmor";
static constexpr const char* g_loadoutName = "NativeLoadout";
static constexpr const char* g_loadoutNoteName = "LoadoutNote";
static constexpr const char* g_loadoutWeaponName = "LoadoutWeapon";
static constexpr const char* g_scratchProbeName = "ScratchProbe";

static constexpr const char* g_loadoutUuid = "0f1e2d3c-4b5a-6978-8796-a5b4c3d2e1f0";

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

Handle<NativeTestArmorAsset> CreateArmor(const char* name)
{
    Handle<NativeTestArmorAsset> armor = MakeHandle<NativeTestArmorAsset>();
    armor->SetName(CreateNameFromDynamicString(name));
    armor->weight = 12.5f;
    armor->defense = 30;

    return armor;
}

Handle<NativeTestLoadoutAsset> CreateLoadout(const char* name, const Handle<NativeTestNoteAsset>& note, const Handle<NativeTestWeaponAsset>& primary)
{
    Handle<NativeTestLoadoutAsset> loadout = MakeHandle<NativeTestLoadoutAsset>();
    loadout->SetName(CreateNameFromDynamicString(name));
    loadout->tag = NAME("Sidearm");
    loadout->serial = 9000000000LL;
    loadout->mass = 0.125;
    loadout->slot = 200;
    loadout->rarity = NativeTestRarity::Legendary;

    loadout->ammoCounts.PushBack(3);
    loadout->ammoCounts.PushBack(5);
    loadout->ammoCounts.PushBack(8);

    loadout->labels.PushBack(String("alpha"));
    loadout->labels.PushBack(String("beta"));

    Handle<NativeTestPickup> plainPickup = MakeHandle<NativeTestPickup>();
    plainPickup->pickupName = "Plain";
    plainPickup->quantity = 1;
    loadout->pickups.PushBack(plainPickup);

    // a subclass of the element type, which has to load back as itself
    Handle<NativeTestMedkit> medkit = MakeHandle<NativeTestMedkit>();
    medkit->pickupName = "Kit";
    medkit->quantity = 2;
    medkit->healAmount = 7.5f;
    loadout->pickups.PushBack(medkit);

    loadout->note = note;
    loadout->primary = primary;

    if (note.IsValid())
    {
        loadout->extraReference = AssetReference(note);
    }

    loadout->scratch = 99;

    if (std::strcmp(name, g_loadoutName) == 0)
    {
        loadout->SetUUID(UUID(g_loadoutUuid));
    }

    return loadout;
}

void WriteManifest(GameTestContext& context, const AssetBucket& bucket, const char* name, const char* text)
{
    const FilePath bucketDirectory = context.GetContentDirectory() / bucket.GetName();

    if (!bucketDirectory.Exists())
    {
        bucketDirectory.MkDir();
    }

    FileByteWriter writer { bucketDirectory / (String(name) + ".hmf") };

    if (!context.Check(writer.IsOpen(), HYP_FORMAT("Failed to open {}/{}.hmf for writing", bucket.GetName(), name)))
    {
        return;
    }

    writer.Write(text, std::strlen(text));
    writer.Close();
}

Handle<AssetObject> ReloadFromDisk(GameTestContext& context, const AssetBucket& bucket, const char* name)
{
    context.GetAssetRegistry()->RemoveCached(bucket, StringHash(name));

    return context.GetAssetRegistry()->GetAsset(bucket, StringHash(name));
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

void DerivedAssetType_ReflectsInheritedMembers(GameTestContext& context)
{
    CheckClass(context, NativeTestGearAsset::StaticClass(), AssetObject::StaticClass(), "NativeTestGearAsset");
    CheckClass(context, NativeTestArmorAsset::StaticClass(), NativeTestGearAsset::StaticClass(), "NativeTestArmorAsset");

    const Class* armorClass = NativeTestArmorAsset::StaticClass();

    context.Check(armorClass->GetField(StringHash("Defense")) != nullptr, "NativeTestArmorAsset has no Defense field");
    context.Check(armorClass->GetField(StringHash("Weight"), /* deep */ true) != nullptr, "NativeTestArmorAsset doesn't inherit Weight");
    context.Check(armorClass->GetAttributeDeep("assetbucket"_sh).IsString(), "NativeTestArmorAsset doesn't inherit the assetbucket attribute");
}

void LoadoutAssetType_ReflectsEveryFieldType(GameTestContext& context)
{
    CheckClass(context, NativeTestLoadoutAsset::StaticClass(), AssetObject::StaticClass(), "NativeTestLoadoutAsset");

    const Class* loadoutClass = NativeTestLoadoutAsset::StaticClass();

    for (const char* fieldName : { "Tag", "Serial", "Mass", "Slot", "Rarity", "AmmoCounts", "Labels", "Pickups", "Note", "Primary", "ExtraReference", "Scratch" })
    {
        context.Check(loadoutClass->GetField(StringHash(fieldName)) != nullptr, HYP_FORMAT("NativeTestLoadoutAsset has no field {}", fieldName));
    }
}

void ArmorAsset_GoesInInheritedBucket(GameTestContext& context)
{
    CheckBucket(context, CreateArmor("BucketProbeArmor"), AssetBuckets::Decals);
}

void ArmorAsset_SavesManifest(GameTestContext& context)
{
    SaveAsset(context, CreateArmor(g_armorName), AssetBuckets::Decals);
}

void LoadoutAsset_SavesManifest(GameTestContext& context)
{
    // the referenced assets are saved first so a new session can resolve them
    Handle<NativeTestNoteAsset> note = MakeHandle<NativeTestNoteAsset>();
    note->SetName(CreateNameFromDynamicString(g_loadoutNoteName));
    note->text = "Loadout note";

    SaveAsset(context, note, AssetBuckets::Misc);

    if (context.HasFailed())
    {
        return;
    }

    Handle<NativeTestWeaponAsset> weapon = MakeHandle<NativeTestWeaponAsset>();
    weapon->SetName(CreateNameFromDynamicString(g_loadoutWeaponName));
    weapon->damage = 9;
    weapon->displayName = "Loadout weapon";

    SaveAsset(context, weapon, AssetBuckets::RawData);

    if (context.HasFailed())
    {
        return;
    }

    SaveAsset(context, CreateLoadout(g_loadoutName, note, weapon), AssetBuckets::RawData);
}

void SavingAsset_RegistersTheAssetsItReferences(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = MakeHandle<NativeTestNoteAsset>();
    note->SetName(NAME("DeepReferencedNote"));
    note->text = "Unregistered reference";

    Handle<NativeTestLoadoutAsset> loadout = CreateLoadout("DeepReferenceLoadout", note, Handle<NativeTestWeaponAsset>());

    if (!context.Check(!note->IsRegistered(), "the referenced note was registered before anything was saved"))
    {
        return;
    }

    context.GetAssetRegistry()->PutAsset(loadout);

    if (Result saveResult = loadout->Save(); !context.Check(!saveResult.HasError(), "Saving DeepReferenceLoadout failed"))
    {
        return;
    }

    if (!context.Check(note->IsRegistered(), "Saving the loadout did not register the note it references"))
    {
        return;
    }

    context.Check(note->GetPath().GetBucket() == AssetBuckets::Misc, "the referenced note went in another bucket than Misc");
}

void ReloadedAsset_MatchesWhatWasSaved(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("RoundTripNote");
    SaveAsset(context, note, AssetBuckets::Misc);

    if (context.HasFailed())
    {
        return;
    }

    Handle<AssetObject> reloaded = ReloadFromDisk(context, AssetBuckets::Misc, "RoundTripNote");

    if (!context.Check(reloaded.IsValid(), "RoundTripNote did not load back from its manifest"))
    {
        return;
    }

    context.Check(reloaded.Get() != static_cast<AssetObject*>(note.Get()), "the reload handed back the object that was cached before");
    context.Check(reloaded->IsRegistered(), "the reloaded note is not registered");
    context.Check(reloaded->GetPath() == note->GetPath(), "the reloaded note has another path");
    context.Check(reloaded->GetUUID() == note->GetUUID(), "the reloaded note has another UUID");

    if (context.Check(reloaded->IsA<NativeTestNoteAsset>(), "the reloaded note is not a NativeTestNoteAsset"))
    {
        context.Check(static_cast<NativeTestNoteAsset*>(reloaded.Get())->text == "Hello traveller", "the reloaded note lost its text");
    }
}

void ReloadedAsset_ResetsTransientFields(GameTestContext& context)
{
    Handle<NativeTestLoadoutAsset> loadout = CreateLoadout(g_scratchProbeName, Handle<NativeTestNoteAsset>(), Handle<NativeTestWeaponAsset>());

    if (!context.Check(loadout->scratch == 99, "the probe's scratch value was not set"))
    {
        return;
    }

    SaveAsset(context, loadout, AssetBuckets::RawData);

    if (context.HasFailed())
    {
        return;
    }

    Handle<AssetObject> reloaded = ReloadFromDisk(context, AssetBuckets::RawData, g_scratchProbeName);

    if (!context.Check(reloaded.IsValid() && reloaded->IsA<NativeTestLoadoutAsset>(), "ScratchProbe did not load back as a NativeTestLoadoutAsset"))
    {
        return;
    }

    const NativeTestLoadoutAsset& probe = *static_cast<NativeTestLoadoutAsset*>(reloaded.Get());

    context.Check(probe.scratch == 7, HYP_FORMAT("scratch: expected its default 7, got {}", probe.scratch));
    context.Check(probe.serial == 9000000000LL, HYP_FORMAT("serial: expected 9000000000, got {}", probe.serial));
}

void ResavingAsset_OverwritesItsManifest(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("ResaveNote");
    SaveAsset(context, note, AssetBuckets::Misc);

    if (context.HasFailed())
    {
        return;
    }

    note->text = "Changed after the first save";

    if (Result saveResult = note->Save(); !context.Check(!saveResult.HasError(), "Saving ResaveNote again failed"))
    {
        return;
    }

    Handle<AssetObject> reloaded = ReloadFromDisk(context, AssetBuckets::Misc, "ResaveNote");

    if (context.Check(reloaded.IsValid() && reloaded->IsA<NativeTestNoteAsset>(), "ResaveNote did not load back"))
    {
        const String& text = static_cast<NativeTestNoteAsset*>(reloaded.Get())->text;

        context.Check(text == "Changed after the first save", HYP_FORMAT("text: expected the second save's, got '{}'", text));
    }
}

void PutAssetUnique_RenamesAClashingName(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> first = CreateNote("ClashNote");
    Handle<NativeTestNoteAsset> second = CreateNote("ClashNote");

    context.GetAssetRegistry()->PutAsset(first);
    context.GetAssetRegistry()->PutAssetUnique(second);

    if (!context.Check(first->IsRegistered() && second->IsRegistered(), "an asset of the clashing pair was not registered"))
    {
        return;
    }

    context.Check(first->GetName() != second->GetName(), HYP_FORMAT("both assets are named {}", first->GetName()));
    context.Check(second->GetName() != NAME("ClashNote"), "the second asset kept the clashing name");

    const Handle<AssetObject> found = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("ClashNote"));

    context.Check(found.Get() == static_cast<AssetObject*>(first.Get()), "ClashNote no longer refers to the first asset");
}

void Registry_HasAssetOnlyOnceRegistered(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("HasAssetNote");

    context.Check(!context.GetAssetRegistry()->HasAsset(AssetBuckets::Misc, StringHash("HasAssetNote")), "HasAsset is true before the note was put");

    context.GetAssetRegistry()->PutAsset(note);

    context.Check(context.GetAssetRegistry()->HasAsset(AssetBuckets::Misc, StringHash("HasAssetNote")), "HasAsset is false after the note was put");
}

void Registry_RemoveAssetForgetsIt(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("RemovedNote");
    context.GetAssetRegistry()->PutAsset(note);

    if (!context.Check(note->IsRegistered(), "RemovedNote was not registered"))
    {
        return;
    }

    context.GetAssetRegistry()->RemoveAsset(note);

    context.Check(!context.GetAssetRegistry()->HasAsset(AssetBuckets::Misc, StringHash("RemovedNote")), "the registry still has RemovedNote");
    context.Check(!note->IsRegistered(), "the removed note still reports itself registered");
    context.Check(!context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("RemovedNote")).IsValid(), "the registry still hands out RemovedNote");
}

void Registry_ScopesLookupsToTheirBucket(GameTestContext& context)
{
    context.GetAssetRegistry()->PutAsset(CreateWeapon("BucketScopedWeapon"));

    context.Check(context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash("BucketScopedWeapon")).IsValid(), "the weapon is not in RawData");
    context.Check(!context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("BucketScopedWeapon")).IsValid(), "the weapon was found in Misc");
}

void Registry_TypedLookupRejectsOtherTypes(GameTestContext& context)
{
    context.GetAssetRegistry()->PutAsset(CreateWeapon("TypedLookupWeapon"));

    context.Check(context.GetAssetRegistry()->GetAsset<NativeTestWeaponAsset>(AssetBuckets::RawData, StringHash("TypedLookupWeapon")).IsValid(), "the typed lookup missed the weapon");
    context.Check(!context.GetAssetRegistry()->GetAsset<NativeTestLoadoutAsset>(AssetBuckets::RawData, StringHash("TypedLookupWeapon")).IsValid(), "the typed lookup returned a weapon as a loadout");
}

void Registry_ReturnsSameObjectForRepeatedLookups(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("CachedLookupNote");
    context.GetAssetRegistry()->PutAsset(note);

    const Handle<AssetObject> first = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("CachedLookupNote"));
    const Handle<AssetObject> second = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("CachedLookupNote"));

    context.Check(first.IsValid() && first.Get() == second.Get(), "two lookups returned different objects");
    context.Check(first.Get() == static_cast<AssetObject*>(note.Get()), "the lookup returned another object than the one that was put");
}

void CloneAsset_CopiesSerializedFields(GameTestContext& context)
{
    Handle<NativeTestLoadoutAsset> source = CreateLoadout("CloneSource", Handle<NativeTestNoteAsset>(), Handle<NativeTestWeaponAsset>());
    context.GetAssetRegistry()->PutAsset(source);

    Handle<AssetObject> clone = source->CloneAsset();

    if (!context.Check(clone.IsValid() && clone->IsA<NativeTestLoadoutAsset>(), "CloneAsset did not return a NativeTestLoadoutAsset"))
    {
        return;
    }

    const NativeTestLoadoutAsset& copy = *static_cast<NativeTestLoadoutAsset*>(clone.Get());

    context.Check(clone.Get() != static_cast<AssetObject*>(source.Get()), "CloneAsset returned the same object");
    context.Check(!clone->IsRegistered(), "the clone is registered");
    context.Check(clone->GetUUID() != source->GetUUID(), "the clone has the same UUID as its source");
    context.Check(copy.serial == 9000000000LL, HYP_FORMAT("clone serial: got {}", copy.serial));
    context.Check(copy.tag == NAME("Sidearm"), "clone lost its tag");
    context.Check(copy.ammoCounts.Size() == 3, HYP_FORMAT("clone ammoCounts: expected 3 elements, got {}", copy.ammoCounts.Size()));
    context.Check(copy.pickups.Size() == 2, HYP_FORMAT("clone pickups: expected 2 elements, got {}", copy.pickups.Size()));
}

void Rename_ChangesNameAndPath(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("BeforeRename");

    note->SetName(NAME("AfterRename"));

    context.Check(note->GetName() == NAME("AfterRename"), "SetName did not rename the unregistered note");

    context.GetAssetRegistry()->PutAsset(note);

    context.Check(note->GetPath().GetName() == NAME("AfterRename"), "the registered path does not carry the new name");
    context.Check(context.GetAssetRegistry()->HasAsset(AssetBuckets::Misc, StringHash("AfterRename")), "the registry does not list the new name");
    context.Check(!context.GetAssetRegistry()->HasAsset(AssetBuckets::Misc, StringHash("BeforeRename")), "the registry lists the old name");
}

void TransientAsset_IsNeverSaved(GameTestContext& context)
{
    Handle<NativeTestNoteAsset> note = CreateNote("TransientNote");
    note->SetIsTransient(true);

    context.Check(note->IsTransient(), "SetIsTransient(true) did not take");

    context.GetAssetRegistry()->PutAsset(note);

    context.Check(!note->IsSaved(), "a transient asset reports itself saved");
    context.Check(!note->IsDirty(), "a transient asset reports itself dirty");
}

// manifests as an older or broken build of the game could have left them
void StaleManifests_AreWritten(GameTestContext& context)
{
    WriteManifest(context, AssetBuckets::Misc, "StaleNote", "NativeTestNoteAsset StaleNote {\n    Text = \"still here\"\n    RemovedField = 5\n}\n");
    WriteManifest(context, AssetBuckets::RawData, "PartialWeapon", "NativeTestWeaponAsset PartialWeapon {\n    Damage = 7\n}\n");
    WriteManifest(context, AssetBuckets::Misc, "GhostAsset", "NativeTestRemovedAsset GhostAsset {\n    Text = \"gone\"\n}\n");
    WriteManifest(context, AssetBuckets::Misc, "NotAnAsset", "NativeTestPickup NotAnAsset {\n    PickupName = \"not an asset\"\n}\n");
    WriteManifest(context, AssetBuckets::Misc, "BrokenSyntax", "NativeTestNoteAsset BrokenSyntax {\n    Text = = =\n");
    WriteManifest(context, AssetBuckets::RawData, "WrongTypeWeapon", "NativeTestWeaponAsset WrongTypeWeapon {\n    Damage = \"not a number\"\n    DisplayName = \"Wrong type\"\n}\n");
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

const NativeTestLoadoutAsset* LoadLoadout(GameTestContext& context, const char* name)
{
    Handle<AssetObject> asset = context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash(name));

    if (!context.Check(asset.IsValid(), HYP_FORMAT("RawData/{} did not load", name)))
    {
        return nullptr;
    }

    if (!context.Check(asset->IsA<NativeTestLoadoutAsset>(), HYP_FORMAT("RawData/{} loaded as {}", name, asset->InstanceClass()->GetName())))
    {
        return nullptr;
    }

    // the registry keeps the loaded asset cached
    return static_cast<NativeTestLoadoutAsset*>(asset.Get());
}

void ArmorAsset_LoadsInheritedFields(GameTestContext& context)
{
    Handle<AssetObject> asset = context.GetAssetRegistry()->GetAsset(AssetBuckets::Decals, StringHash(g_armorName));

    if (!context.Check(asset.IsValid(), HYP_FORMAT("Decals/{} did not load", g_armorName)))
    {
        return;
    }

    if (!context.Check(asset->IsA<NativeTestArmorAsset>(), HYP_FORMAT("Decals/{} loaded as {}", g_armorName, asset->InstanceClass()->GetName())))
    {
        return;
    }

    const NativeTestArmorAsset& armor = *static_cast<NativeTestArmorAsset*>(asset.Get());

    context.Check(armor.weight == 12.5f, HYP_FORMAT("weight, declared by the base class: expected 12.5, got {}", armor.weight));
    context.Check(armor.defense == 30, HYP_FORMAT("defense: expected 30, got {}", armor.defense));
}

void LoadoutAsset_LoadsEveryFieldType(GameTestContext& context)
{
    const NativeTestLoadoutAsset* loadout = LoadLoadout(context, g_loadoutName);

    if (!loadout)
    {
        return;
    }

    context.Check(loadout->tag == NAME("Sidearm"), HYP_FORMAT("tag: expected Sidearm, got {}", loadout->tag));
    context.Check(loadout->serial == 9000000000LL, HYP_FORMAT("serial: expected 9000000000, got {}", loadout->serial));
    context.Check(loadout->mass == 0.125, HYP_FORMAT("mass: expected 0.125, got {}", loadout->mass));
    context.Check(loadout->slot == 200, HYP_FORMAT("slot: expected 200, got {}", uint32(loadout->slot)));
    context.Check(loadout->rarity == NativeTestRarity::Legendary, HYP_FORMAT("rarity: expected Legendary (3), got {}", uint32(loadout->rarity)));

    if (context.Check(loadout->ammoCounts.Size() == 3, HYP_FORMAT("ammoCounts: expected 3 elements, got {}", loadout->ammoCounts.Size())))
    {
        context.Check(loadout->ammoCounts[0] == 3 && loadout->ammoCounts[1] == 5 && loadout->ammoCounts[2] == 8,
            HYP_FORMAT("ammoCounts: expected 3, 5, 8, got {}, {}, {}", loadout->ammoCounts[0], loadout->ammoCounts[1], loadout->ammoCounts[2]));
    }

    if (context.Check(loadout->labels.Size() == 2, HYP_FORMAT("labels: expected 2 elements, got {}", loadout->labels.Size())))
    {
        context.Check(loadout->labels[0] == "alpha" && loadout->labels[1] == "beta", "labels: expected alpha, beta");
    }
}

void LoadoutAsset_LoadsPolymorphicArrayElements(GameTestContext& context)
{
    const NativeTestLoadoutAsset* loadout = LoadLoadout(context, g_loadoutName);

    if (!loadout || !context.Check(loadout->pickups.Size() == 2, "pickups: expected 2 elements"))
    {
        return;
    }

    if (!context.Check(loadout->pickups[0].IsValid() && loadout->pickups[1].IsValid(), "a pickup element did not load"))
    {
        return;
    }

    context.Check(loadout->pickups[0]->pickupName == "Plain", HYP_FORMAT("pickups[0]: expected 'Plain', got '{}'", loadout->pickups[0]->pickupName));
    context.Check(loadout->pickups[0]->InstanceClass() == NativeTestPickup::StaticClass(), "pickups[0] is not a plain NativeTestPickup");

    if (context.Check(loadout->pickups[1]->IsA<NativeTestMedkit>(), HYP_FORMAT("pickups[1] loaded as {}, expected NativeTestMedkit", loadout->pickups[1]->InstanceClass()->GetName())))
    {
        context.Check(static_cast<NativeTestMedkit*>(loadout->pickups[1].Get())->healAmount == 7.5f, "pickups[1].healAmount: expected 7.5");
    }
}

void LoadoutAsset_ResolvesReferencesToTheRegisteredAssets(GameTestContext& context)
{
    const NativeTestLoadoutAsset* loadout = LoadLoadout(context, g_loadoutName);

    if (!loadout)
    {
        return;
    }

    const Handle<AssetObject> registeredNote = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash(g_loadoutNoteName));
    const Handle<AssetObject> registeredWeapon = context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash(g_loadoutWeaponName));

    if (!context.Check(registeredNote.IsValid() && registeredWeapon.IsValid(), "the assets the loadout references did not load"))
    {
        return;
    }

    if (context.Check(loadout->note.IsValid(), "note did not load"))
    {
        context.Check(loadout->note.Get() == static_cast<NativeTestNoteAsset*>(registeredNote.Get()), "note is another object than the registry's Misc/LoadoutNote");
        context.Check(loadout->note->text == "Loadout note", HYP_FORMAT("note.text: got '{}'", loadout->note->text));
    }

    if (context.Check(loadout->primary.IsValid(), "primary did not load"))
    {
        context.Check(loadout->primary.Get() == static_cast<NativeTestWeaponAsset*>(registeredWeapon.Get()), "primary is another object than the registry's RawData/LoadoutWeapon");
        context.Check(loadout->primary->damage == 9, HYP_FORMAT("primary.damage: expected 9, got {}", loadout->primary->damage));
    }

    if (context.Check(loadout->extraReference.IsValid(), "extraReference did not load"))
    {
        context.Check(loadout->extraReference.GetAssetPath() == registeredNote->GetPath(), "extraReference refers to another path than Misc/LoadoutNote");
        context.Check(loadout->extraReference.Resolve().Get() == registeredNote.Get(), "extraReference resolves to another object than the registry's Misc/LoadoutNote");
    }
}

void LoadoutAsset_ResetsTransientFields(GameTestContext& context)
{
    const NativeTestLoadoutAsset* probe = LoadLoadout(context, g_scratchProbeName);

    if (!probe)
    {
        return;
    }

    context.Check(probe->scratch == 7, HYP_FORMAT("scratch: expected its default 7, got {}", probe->scratch));
    context.Check(probe->serial == 9000000000LL, HYP_FORMAT("serial: expected 9000000000, got {}", probe->serial));
}

void LoadedAsset_KeepsItsPathAndUuid(GameTestContext& context)
{
    const NativeTestLoadoutAsset* loadout = LoadLoadout(context, g_loadoutName);

    if (!loadout || !context.Check(loadout->IsRegistered(), "the loaded loadout is not registered"))
    {
        return;
    }

    context.Check(loadout->GetPath().GetBucket() == AssetBuckets::RawData, "the loaded loadout is not in RawData");
    context.Check(loadout->GetPath().GetName() == NAME("NativeLoadout"), "the loaded loadout has another name in its path");
    context.Check(loadout->GetUUID() == UUID(g_loadoutUuid), "the loaded loadout has another UUID than the one it was saved with");
}

void LoadedAsset_IsNotDirtyUntilMarked(GameTestContext& context)
{
    const NativeTestLoadoutAsset* loadout = LoadLoadout(context, g_loadoutName);

    if (!loadout)
    {
        return;
    }

    context.Check(!loadout->IsDirty(), "an asset that was only loaded is dirty");

    const_cast<NativeTestLoadoutAsset*>(loadout)->MarkDirty();

    context.Check(loadout->IsDirty(), "MarkDirty did not mark the loaded asset dirty");
}

void Registry_ListsAssetsSavedByThePreviousSession(GameTestContext& context)
{
    Array<AssetDesc> rawDataDescs;
    context.GetAssetRegistry()->GetBucketAssetDescs(AssetBuckets::RawData.GetIndex(), rawDataDescs);

    for (const char* expected : { g_weaponName, g_loadoutName, g_loadoutWeaponName, g_scratchProbeName })
    {
        bool found = false;

        for (const AssetDesc& desc : rawDataDescs)
        {
            found |= desc.name == CreateNameFromDynamicString(expected);
        }

        context.Check(found, HYP_FORMAT("RawData does not list {}", expected));
    }

    Array<AssetDesc> decalDescs;

    context.Check(context.GetAssetRegistry()->GetBucketAssetDescs(AssetBuckets::Decals.GetIndex(), decalDescs) == 1,
        HYP_FORMAT("Decals asset count: expected 1, got {}", decalDescs.Size()));
}

void Registry_ReturnsSameObjectAcrossLookupsAfterLoading(GameTestContext& context)
{
    const Handle<AssetObject> first = context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash(g_loadoutName));
    const Handle<AssetObject> second = context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash(g_loadoutName));

    context.Check(first.IsValid() && first.Get() == second.Get(), "two lookups of a loaded asset returned different objects");
}

void StaleManifest_LoadsWithoutItsRemovedField(GameTestContext& context)
{
    const Handle<NativeTestNoteAsset> note = context.GetAssetRegistry()->GetAsset<NativeTestNoteAsset>(AssetBuckets::Misc, StringHash("StaleNote"));

    if (context.Check(note.IsValid(), "Misc/StaleNote did not load"))
    {
        context.Check(note->text == "still here", HYP_FORMAT("text: expected 'still here', got '{}'", note->text));
    }
}

void PartialManifest_KeepsDefaultsForMissingFields(GameTestContext& context)
{
    const Handle<NativeTestWeaponAsset> weapon = context.GetAssetRegistry()->GetAsset<NativeTestWeaponAsset>(AssetBuckets::RawData, StringHash("PartialWeapon"));

    if (!context.Check(weapon.IsValid(), "RawData/PartialWeapon did not load"))
    {
        return;
    }

    context.Check(weapon->damage == 7, HYP_FORMAT("damage: expected 7, got {}", weapon->damage));
    context.Check(weapon->fireRate == 0.0f, HYP_FORMAT("fireRate: expected its default 0, got {}", weapon->fireRate));
    context.Check(weapon->displayName.Empty(), HYP_FORMAT("displayName: expected empty, got '{}'", weapon->displayName));
    context.Check(!weapon->isAutomatic, "isAutomatic: expected its default false");
    context.Check(weapon->rarity == NativeTestRarity::Common, "rarity: expected its default Common");
    context.Check(!weapon->ammo.IsValid(), "ammo: expected null");
}

void ManifestOfRemovedClass_DoesNotLoad(GameTestContext& context)
{
    context.Check(!context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("GhostAsset")).IsValid(), "Misc/GhostAsset loaded though its class does not exist");
}

void ManifestOfNonAssetClass_DoesNotLoad(GameTestContext& context)
{
    context.Check(!context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("NotAnAsset")).IsValid(), "Misc/NotAnAsset loaded though NativeTestPickup is not an AssetObject");
}

void ManifestWithBrokenSyntax_DoesNotBreakTheRegistry(GameTestContext& context)
{
    // the parser recovers from syntax errors, so the asset may load with what could be read
    const Handle<AssetObject> asset = context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash("BrokenSyntax"));

    if (asset.IsValid())
    {
        context.Check(asset->IsA<NativeTestNoteAsset>(), HYP_FORMAT("Misc/BrokenSyntax loaded as {}", asset->InstanceClass()->GetName()));
    }

    context.Check(context.GetAssetRegistry()->GetAsset(AssetBuckets::Misc, StringHash(g_noteName)).IsValid(), HYP_FORMAT("Misc/{} no longer loads", g_noteName));
}

void ManifestWithWrongFieldType_DoesNotBreakTheRegistry(GameTestContext& context)
{
    const Handle<AssetObject> asset = context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash("WrongTypeWeapon"));

    if (asset.IsValid())
    {
        context.Check(asset->IsA<NativeTestWeaponAsset>(), HYP_FORMAT("RawData/WrongTypeWeapon loaded as {}", asset->InstanceClass()->GetName()));
    }

    // the registry still serves the assets around it
    context.Check(context.GetAssetRegistry()->GetAsset(AssetBuckets::RawData, StringHash(g_weaponName)).IsValid(), HYP_FORMAT("RawData/{} no longer loads", g_weaponName));
}

void UnloadableManifests_AreStillListed(GameTestContext& context)
{
    for (const char* expected : { g_noteName, "StaleNote", "GhostAsset", "NotAnAsset", "BrokenSyntax" })
    {
        context.Check(context.GetAssetRegistry()->HasAsset(AssetBuckets::Misc, StringHash(expected)), HYP_FORMAT("Misc does not list {}", expected));
    }
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
            { "DerivedAssetType_ReflectsInheritedMembers", g_writePhase, &DerivedAssetType_ReflectsInheritedMembers },
            { "LoadoutAssetType_ReflectsEveryFieldType", g_writePhase, &LoadoutAssetType_ReflectsEveryFieldType },
            { "ArmorAsset_GoesInInheritedBucket", g_writePhase, &ArmorAsset_GoesInInheritedBucket },
            { "ArmorAsset_SavesManifest", g_writePhase, &ArmorAsset_SavesManifest },
            { "LoadoutAsset_SavesManifest", g_writePhase, &LoadoutAsset_SavesManifest },
            { "SavingAsset_RegistersTheAssetsItReferences", g_writePhase, &SavingAsset_RegistersTheAssetsItReferences },
            { "ReloadedAsset_MatchesWhatWasSaved", g_writePhase, &ReloadedAsset_MatchesWhatWasSaved },
            { "ReloadedAsset_ResetsTransientFields", g_writePhase, &ReloadedAsset_ResetsTransientFields },
            { "ResavingAsset_OverwritesItsManifest", g_writePhase, &ResavingAsset_OverwritesItsManifest },
            { "PutAssetUnique_RenamesAClashingName", g_writePhase, &PutAssetUnique_RenamesAClashingName },
            { "Registry_HasAssetOnlyOnceRegistered", g_writePhase, &Registry_HasAssetOnlyOnceRegistered },
            { "Registry_RemoveAssetForgetsIt", g_writePhase, &Registry_RemoveAssetForgetsIt },
            { "Registry_ScopesLookupsToTheirBucket", g_writePhase, &Registry_ScopesLookupsToTheirBucket },
            { "Registry_TypedLookupRejectsOtherTypes", g_writePhase, &Registry_TypedLookupRejectsOtherTypes },
            { "Registry_ReturnsSameObjectForRepeatedLookups", g_writePhase, &Registry_ReturnsSameObjectForRepeatedLookups },
            { "CloneAsset_CopiesSerializedFields", g_writePhase, &CloneAsset_CopiesSerializedFields },
            { "Rename_ChangesNameAndPath", g_writePhase, &Rename_ChangesNameAndPath },
            { "TransientAsset_IsNeverSaved", g_writePhase, &TransientAsset_IsNeverSaved },
            { "StaleManifests_AreWritten", g_writePhase, &StaleManifests_AreWritten },
            { "WeaponAsset_LoadsSavedFields", g_readPhase, &WeaponAsset_LoadsSavedFields },
            { "NoteAsset_LoadsSavedFields", g_readPhase, &NoteAsset_LoadsSavedFields },
            { "ArmorAsset_LoadsInheritedFields", g_readPhase, &ArmorAsset_LoadsInheritedFields },
            { "LoadoutAsset_LoadsEveryFieldType", g_readPhase, &LoadoutAsset_LoadsEveryFieldType },
            { "LoadoutAsset_LoadsPolymorphicArrayElements", g_readPhase, &LoadoutAsset_LoadsPolymorphicArrayElements },
            { "LoadoutAsset_ResolvesReferencesToTheRegisteredAssets", g_readPhase, &LoadoutAsset_ResolvesReferencesToTheRegisteredAssets },
            { "LoadoutAsset_ResetsTransientFields", g_readPhase, &LoadoutAsset_ResetsTransientFields },
            { "LoadedAsset_KeepsItsPathAndUuid", g_readPhase, &LoadedAsset_KeepsItsPathAndUuid },
            { "LoadedAsset_IsNotDirtyUntilMarked", g_readPhase, &LoadedAsset_IsNotDirtyUntilMarked },
            { "Registry_ListsAssetsSavedByThePreviousSession", g_readPhase, &Registry_ListsAssetsSavedByThePreviousSession },
            { "Registry_ReturnsSameObjectAcrossLookupsAfterLoading", g_readPhase, &Registry_ReturnsSameObjectAcrossLookupsAfterLoading },
            { "StaleManifest_LoadsWithoutItsRemovedField", g_readPhase, &StaleManifest_LoadsWithoutItsRemovedField },
            { "PartialManifest_KeepsDefaultsForMissingFields", g_readPhase, &PartialManifest_KeepsDefaultsForMissingFields },
            { "ManifestOfRemovedClass_DoesNotLoad", g_readPhase, &ManifestOfRemovedClass_DoesNotLoad },
            { "ManifestOfNonAssetClass_DoesNotLoad", g_readPhase, &ManifestOfNonAssetClass_DoesNotLoad },
            { "ManifestWithBrokenSyntax_DoesNotBreakTheRegistry", g_readPhase, &ManifestWithBrokenSyntax_DoesNotBreakTheRegistry },
            { "ManifestWithWrongFieldType_DoesNotBreakTheRegistry", g_readPhase, &ManifestWithWrongFieldType_DoesNotBreakTheRegistry },
            { "UnloadableManifests_AreStillListed", g_readPhase, &UnloadableManifests_AreStillListed },
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
