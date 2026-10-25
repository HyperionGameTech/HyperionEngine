using Hyperion;

namespace GameTests
{
    public static class AssetTypeTests
    {
        public const string ArmorName = "HarnessArmor";
        public const string LoadoutName = "HarnessLoadout";
        public const string LoadoutNoteName = "LoadoutNote";
        public const string LoadoutWeaponName = "LoadoutWeapon";
        public const string ScratchProbeName = "ScratchProbe";

        [GameTest]
        public static void DerivedAssetType_ReflectsInheritedMembers(GameTestContext context)
        {
            Check.True(Class.TryGetDynamicClass(typeof(TestArmorAsset), out Class armorClass), "TestArmorAsset has no dynamic class");
            Check.True(Class.TryGetDynamicClass(typeof(TestGearAsset), out Class gearClass), "TestGearAsset has no dynamic class");

            Check.True(armorClass.IsSubclassOf(gearClass), "TestArmorAsset class does not derive from TestGearAsset");
            Check.True(armorClass.IsSubclassOf(Class.GetClass(typeof(AssetObject))), "TestArmorAsset class does not derive from AssetObject");
            Check.True(armorClass.GetProperty(new Name("Defense")) != null, "TestArmorAsset class has no Defense property");
            Check.True(gearClass.GetProperty(new Name("Weight")) != null, "TestGearAsset class has no Weight property");
        }

        [GameTest]
        public static void LoadoutType_ReflectsEveryFieldType(GameTestContext context)
        {
            Check.True(Class.TryGetDynamicClass(typeof(TestLoadoutAsset), out Class cls), "TestLoadoutAsset has no dynamic class");

            foreach (string propertyName in new[] { "Tag", "Serial", "Mass", "Slot", "Rarity", "Note", "Primary", "Spare", "Scratch" })
            {
                Check.True(cls.GetProperty(new Name(propertyName)) != null, $"TestLoadoutAsset class has no {propertyName} property");
            }
        }

        [GameTest]
        public static void DerivedAsset_GoesInInheritedBucket(GameTestContext context)
        {
            TestArmorAsset armor = CreateArmor("BucketProbeArmor");
            context.AssetRegistry.PutAsset(armor);

            Check.True(armor.IsRegistered(), "TestArmorAsset was not registered");
            Check.Equal(AssetBucket.Decals.Index, armor.Path.BucketIndex, "TestArmorAsset bucket");
        }

        [GameTest]
        public static void DerivedAsset_SavesManifest(GameTestContext context)
        {
            SaveAsset(context, CreateArmor(ArmorName), AssetBucket.Decals);
        }

        [GameTest]
        public static void LoadoutAsset_SavesManifest(GameTestContext context)
        {
            // the referenced assets are saved first so a new session can resolve them
            TestNoteAsset note = new TestNoteAsset { Text = "Loadout note" };
            note.SetName(new Name(LoadoutNoteName));
            SaveAsset(context, note, AssetBucket.Misc);

            TestWeaponAsset weapon = new TestWeaponAsset { Damage = 9, DisplayName = "Loadout weapon" };
            weapon.SetName(new Name(LoadoutWeaponName));
            SaveAsset(context, weapon, AssetBucket.RawData);

            TestLoadoutAsset loadout = CreateLoadout(LoadoutName, note, weapon);
            SaveAsset(context, loadout, AssetBucket.RawData);
        }

        [GameTest]
        public static void SavingAsset_RegistersTheAssetsItReferences(GameTestContext context)
        {
            TestNoteAsset note = new TestNoteAsset { Text = "Unregistered reference" };
            note.SetName(new Name("DeepReferencedNote"));

            TestLoadoutAsset loadout = CreateLoadout("DeepReferenceLoadout", note, null);
            Check.True(!note.IsRegistered(), "the referenced note was registered before anything was saved");

            context.AssetRegistry.PutAsset(loadout);
            Check.True(loadout.Save().IsValid, "Saving DeepReferenceLoadout failed, see the log");

            Check.True(note.IsRegistered(), "Saving the loadout did not register the note it references");
            Check.Equal(AssetBucket.Misc.Index, note.Path.BucketIndex, "referenced note bucket");
        }

        [GameTest]
        public static void TransientField_IsNotSaved(GameTestContext context)
        {
            TestLoadoutAsset loadout = CreateLoadout(ScratchProbeName, null, null);
            loadout.Scratch = 99;

            SaveAsset(context, loadout, AssetBucket.RawData);

            string manifest = File.ReadAllText(Path.Combine(context.ContentDirectory, AssetBucket.RawData.Name, ScratchProbeName + ".hmf"));
            Check.True(!manifest.Contains("Scratch ="), "the manifest contains the Transient Scratch field");
            Check.True(manifest.Contains("Serial"), "the manifest is missing the serialized Serial field");
        }

        [GameTest]
        public static void SavedManifest_NamesItsClass(GameTestContext context)
        {
            SaveAsset(context, CreateLoadout("ManifestHeaderProbe", null, null), AssetBucket.RawData);

            string manifest = File.ReadAllText(Path.Combine(context.ContentDirectory, AssetBucket.RawData.Name, "ManifestHeaderProbe.hmf"));
            Check.True(manifest.StartsWith("TestLoadoutAsset ManifestHeaderProbe"), $"manifest starts with '{manifest.Substring(0, Math.Min(manifest.Length, 60))}'");
        }

        [GameTest]
        public static void CloneAsset_CopiesSerializedFields(GameTestContext context)
        {
            TestWeaponAsset weapon = new TestWeaponAsset { Damage = 17, DisplayName = "Cloned", MuzzleOffset = new Vec3f(4.0f, 5.0f, 6.0f) };
            weapon.SetName(new Name("CloneSource"));
            context.AssetRegistry.PutAsset(weapon);

            TestWeaponAsset? clone = weapon.CloneAsset() as TestWeaponAsset;

            Check.True(clone != null, "CloneAsset did not return a TestWeaponAsset");
            Check.True(clone!.NativeAddress != weapon.NativeAddress, "CloneAsset returned the same object");
            Check.True(!clone.IsRegistered(), "the clone is registered");
            Check.True(!clone.GetUUID().Equals(weapon.GetUUID()), "the clone has the same UUID as its source");
            Check.Equal(17, clone.Damage, "clone Damage");
            Check.Equal("Cloned", clone.DisplayName, "clone DisplayName");
            Check.Equal(5.0f, clone.MuzzleOffset.y, "clone MuzzleOffset.y");
        }

        [GameTest]
        public static void PutAssetUnique_RenamesAClashingName(GameTestContext context)
        {
            TestNoteAsset first = new TestNoteAsset { Text = "first" };
            first.SetName(new Name("ClashNote"));
            context.AssetRegistry.PutAsset(first);

            TestNoteAsset second = new TestNoteAsset { Text = "second" };
            second.SetName(new Name("ClashNote"));
            context.AssetRegistry.PutAssetUnique(second);

            Check.True(first.IsRegistered() && second.IsRegistered(), "an asset of the clashing pair was not registered");
            Check.True(first.GetName() != second.GetName(), $"both assets are named {first.GetName()}");
            Check.True(second.GetName() != "ClashNote", "the second asset kept the clashing name");

            TestNoteAsset? found = context.AssetRegistry.GetAsset<TestNoteAsset>(AssetBucket.Misc, new Name("ClashNote"));
            Check.True(found != null && found.NativeAddress == first.NativeAddress, "ClashNote no longer refers to the first asset");
        }

        [GameTest]
        public static void Registry_ReturnsSameObjectForRepeatedLookups(GameTestContext context)
        {
            TestNoteAsset note = new TestNoteAsset { Text = "cached" };
            note.SetName(new Name("CachedLookupNote"));
            context.AssetRegistry.PutAsset(note);

            AssetObject? first = context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name("CachedLookupNote"));
            AssetObject? second = context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name("CachedLookupNote"));

            Check.True(first != null && second != null, "CachedLookupNote was not found");
            Check.True(first!.NativeAddress == second!.NativeAddress, "two lookups returned different objects");
            Check.True(first.NativeAddress == note.NativeAddress, "the lookup returned another object than the one that was put");
        }

        [GameTest]
        public static void Registry_ScopesLookupsToTheirBucket(GameTestContext context)
        {
            TestWeaponAsset weapon = new TestWeaponAsset { Damage = 1 };
            weapon.SetName(new Name("BucketScopedWeapon"));
            context.AssetRegistry.PutAsset(weapon);

            Check.True(context.AssetRegistry.GetAsset(AssetBucket.RawData.Index, new Name("BucketScopedWeapon")) != null, "the weapon is not in RawData");
            Check.True(context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name("BucketScopedWeapon")) == null, "the weapon was found in Misc");
        }

        [GameTest]
        public static void Registry_TypedLookupRejectsOtherTypes(GameTestContext context)
        {
            TestWeaponAsset weapon = new TestWeaponAsset { Damage = 1 };
            weapon.SetName(new Name("TypedLookupWeapon"));
            context.AssetRegistry.PutAsset(weapon);

            Check.True(context.AssetRegistry.GetAsset<TestWeaponAsset>(AssetBucket.RawData, new Name("TypedLookupWeapon")) != null, "the typed lookup missed the weapon");
            Check.True(context.AssetRegistry.GetAsset<TestLoadoutAsset>(AssetBucket.RawData, new Name("TypedLookupWeapon")) == null, "the typed lookup returned a weapon as a loadout");
        }

        [GameTest]
        public static void Rename_ChangesNameAndPathOnceRegistered(GameTestContext context)
        {
            TestNoteAsset note = new TestNoteAsset { Text = "renamed" };
            note.Name = new Name("BeforeRename");

            Check.Equal("BeforeRename", note.GetName().ToString(), "name before registering");

            note.Name = new Name("AfterRename");
            Check.Equal("AfterRename", note.GetName().ToString(), "name after rename");

            context.AssetRegistry.PutAsset(note);
            Check.Equal("Misc/AfterRename", note.Path.ToString(), "path after registering");
        }

        [GameTest]
        public static void Transient_AssetIsNeverSaved(GameTestContext context)
        {
            TestNoteAsset note = new TestNoteAsset { Text = "transient" };
            note.SetName(new Name("TransientNote"));
            note.SetIsTransient(true);

            Check.True(note.IsTransient(), "SetIsTransient(true) did not take");

            context.AssetRegistry.PutAsset(note);
            Check.True(!note.IsSaved(), "a transient asset reports itself saved");
            Check.True(!note.IsDirty(), "a transient asset reports itself dirty");
        }

        [GameTest(GamePhases.Read)]
        public static void DerivedAsset_LoadsInheritedFields(GameTestContext context)
        {
            AssetObject? asset = context.AssetRegistry.GetAsset(AssetBucket.Decals.Index, new Name(ArmorName));
            Check.NotNull(asset, $"Decals/{ArmorName}");

            TestArmorAsset? armor = asset as TestArmorAsset;
            Check.True(armor != null, $"Decals/{ArmorName} loaded as {asset!.GetType().Name}, not TestArmorAsset");
            Check.Equal(12.5f, armor!.Weight, "Weight, declared by the base class");
            Check.Equal(30, armor.Defense, "Defense");
        }

        [GameTest(GamePhases.Read)]
        public static void LoadoutAsset_LoadsEveryFieldType(GameTestContext context)
        {
            TestLoadoutAsset loadout = LoadLoadout(context, LoadoutName);

            Check.Equal("Sidearm", loadout.Tag.ToString(), "Tag");
            Check.Equal(9_000_000_000L, loadout.Serial, "Serial");
            Check.Equal(0.125, loadout.Mass, "Mass");
            Check.Equal((byte)200, loadout.Slot, "Slot");
            Check.Equal(TestRarity.Legendary, loadout.Rarity, "Rarity");

            TestPickup spare = Check.NotNull(loadout.Spare, "Spare");
            Check.Equal("Spare Cell", spare.PickupName, "Spare.PickupName");
            Check.Equal(4, spare.Quantity, "Spare.Quantity");
        }

        [GameTest(GamePhases.Read)]
        public static void LoadoutAsset_ResolvesReferencesToTheRegisteredAssets(GameTestContext context)
        {
            TestLoadoutAsset loadout = LoadLoadout(context, LoadoutName);

            TestNoteAsset note = Check.NotNull(loadout.Note, "Note");
            TestWeaponAsset weapon = Check.NotNull(loadout.Primary, "Primary");

            TestNoteAsset registeredNote = Check.NotNull(context.AssetRegistry.GetAsset<TestNoteAsset>(AssetBucket.Misc, new Name(LoadoutNoteName)), $"Misc/{LoadoutNoteName}");
            TestWeaponAsset registeredWeapon = Check.NotNull(context.AssetRegistry.GetAsset<TestWeaponAsset>(AssetBucket.RawData, new Name(LoadoutWeaponName)), $"RawData/{LoadoutWeaponName}");

            Check.True(note.NativeAddress == registeredNote.NativeAddress, "Note is another object than the registry's Misc/" + LoadoutNoteName);
            Check.True(weapon.NativeAddress == registeredWeapon.NativeAddress, "Primary is another object than the registry's RawData/" + LoadoutWeaponName);
            Check.Equal("Loadout note", note.Text, "Note.Text");
            Check.Equal(9, weapon.Damage, "Primary.Damage");
        }

        [GameTest(GamePhases.Read)]
        public static void LoadoutAsset_ResetsTransientFields(GameTestContext context)
        {
            TestLoadoutAsset probe = LoadLoadout(context, ScratchProbeName);

            Check.Equal(7, probe.Scratch, "Scratch comes back as its default, not the 99 it was saved with");
            Check.Equal(9_000_000_000L, probe.Serial, "Serial");
        }

        [GameTest(GamePhases.Read)]
        public static void LoadedAsset_KeepsItsPathAndUuid(GameTestContext context)
        {
            TestLoadoutAsset loadout = LoadLoadout(context, LoadoutName);

            Check.True(loadout.IsRegistered(), "the loaded loadout is not registered");
            Check.Equal($"RawData/{LoadoutName}", loadout.Path.ToString(), "path");
            Check.Equal(LoadoutUuid.ToString(), loadout.GetUUID().ToString(), "UUID");
        }

        [GameTest(GamePhases.Read)]
        public static void LoadedAsset_IsNotDirty(GameTestContext context)
        {
            TestLoadoutAsset loadout = LoadLoadout(context, LoadoutName);

            Check.True(!loadout.IsDirty(), "an asset that was only loaded is dirty");

            loadout.Serial = 1;
            loadout.MarkDirty();

            Check.True(loadout.IsDirty(), "MarkDirty did not mark the loaded asset dirty");
        }

        [GameTest(GamePhases.Read)]
        public static void Registry_ListsAssetsSavedByThePreviousSession(GameTestContext context)
        {
            HashSet<string> rawDataNames = context.AssetRegistry.GetBucketAssetDescs(AssetBucket.RawData.Index).Select(desc => desc.Name.ToString()).ToHashSet();

            foreach (string expected in new[] { AssetTests.WeaponName, LoadoutName, LoadoutWeaponName, ScratchProbeName })
            {
                Check.True(rawDataNames.Contains(expected), $"RawData does not list {expected} (lists: {string.Join(", ", rawDataNames)})");
            }

            Check.Equal(1u, context.AssetRegistry.GetBucketAssetCount(AssetBucket.Decals.Index), "Decals asset count");
        }

        private static readonly UUID LoadoutUuid = UUID.Parse("0f1e2d3c-4b5a-6978-8796-a5b4c3d2e1f0");

        private static TestArmorAsset CreateArmor(string name)
        {
            TestArmorAsset armor = new TestArmorAsset { Weight = 12.5f, Defense = 30 };
            armor.SetName(new Name(name));

            return armor;
        }

        private static TestLoadoutAsset CreateLoadout(string name, TestNoteAsset? note, TestWeaponAsset? primary)
        {
            TestLoadoutAsset loadout = new TestLoadoutAsset
            {
                Tag = new Name("Sidearm"),
                Serial = 9_000_000_000L,
                Mass = 0.125,
                Slot = 200,
                Rarity = TestRarity.Legendary,
                Note = note,
                Primary = primary,
                Spare = new TestPickup { PickupName = "Spare Cell", Quantity = 4 }
            };

            loadout.SetName(new Name(name));

            if (name == LoadoutName)
            {
                loadout.SetUUID(LoadoutUuid);
            }

            return loadout;
        }

        private static TestLoadoutAsset LoadLoadout(GameTestContext context, string name)
        {
            AssetObject? asset = context.AssetRegistry.GetAsset(AssetBucket.RawData.Index, new Name(name));
            Check.NotNull(asset, $"RawData/{name}");

            TestLoadoutAsset? loadout = asset as TestLoadoutAsset;
            Check.True(loadout != null, $"RawData/{name} loaded as {asset!.GetType().Name}, not TestLoadoutAsset");

            return loadout!;
        }

        private static void SaveAsset(GameTestContext context, AssetObject asset, AssetBucket expectedBucket)
        {
            Directory.CreateDirectory(Path.Combine(context.ContentDirectory, expectedBucket.Name));

            context.AssetRegistry.PutAsset(asset);

            Check.Equal(expectedBucket.Index, asset.Path.BucketIndex, $"{asset.GetName()} bucket");
            Check.True(asset.Save().IsValid, $"Saving {asset.GetName()} failed, see the log");

            string manifestPath = Path.Combine(context.ContentDirectory, expectedBucket.Name, asset.GetName() + ".hmf");
            Check.True(File.Exists(manifestPath), $"{manifestPath} was not written");
        }
    }
}
