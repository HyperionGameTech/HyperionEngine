using Hyperion;

namespace GameTests
{
    public static class AssetTests
    {
        public const string WeaponName = "HarnessBlaster";
        public const string NoteName = "HarnessNote";

        [GameTest]
        public static void WeaponAsset_GoesInAttributeBucket(GameTestContext context)
        {
            TestWeaponAsset weapon = CreateWeapon("BucketProbeWeapon");
            context.AssetRegistry.PutAsset(weapon);

            Check.True(weapon.IsRegistered(), "TestWeaponAsset was not registered");
            Check.Equal(AssetBucket.RawData.Index, weapon.Path.BucketIndex, "TestWeaponAsset bucket");
        }

        [GameTest]
        public static void NoteAsset_GoesInMiscBucket(GameTestContext context)
        {
            TestNoteAsset note = CreateNote("BucketProbeNote");
            context.AssetRegistry.PutAsset(note);

            Check.True(note.IsRegistered(), "TestNoteAsset was not registered");
            Check.Equal(AssetBucket.Misc.Index, note.Path.BucketIndex, "TestNoteAsset bucket");
        }

        [GameTest]
        public static void WeaponAsset_SavesManifest(GameTestContext context)
        {
            SaveAsset(context, CreateWeapon(WeaponName), AssetBucket.RawData);
        }

        [GameTest]
        public static void NoteAsset_SavesManifest(GameTestContext context)
        {
            SaveAsset(context, CreateNote(NoteName), AssetBucket.Misc);
        }

        [GameTest(GamePhases.Read)]
        public static void WeaponAsset_LoadsSavedFields(GameTestContext context)
        {
            AssetObject? asset = context.AssetRegistry.GetAsset(AssetBucket.RawData.Index, new Name(WeaponName));
            Check.NotNull(asset, $"RawData/{WeaponName}");

            TestWeaponAsset? weapon = asset as TestWeaponAsset;
            Check.True(weapon != null, $"RawData/{WeaponName} loaded as {asset!.GetType().Name}, not TestWeaponAsset");

            Check.Equal(42, weapon!.Damage, "Damage");
            Check.Equal(2.5f, weapon.FireRate, "FireRate");
            Check.Equal("Harness Blaster", weapon.DisplayName, "DisplayName");
            Check.Equal(true, weapon.IsAutomatic, "IsAutomatic");
            Check.Equal(1.0f, weapon.MuzzleOffset.x, "MuzzleOffset.x");
            Check.Equal(2.0f, weapon.MuzzleOffset.y, "MuzzleOffset.y");
            Check.Equal(3.0f, weapon.MuzzleOffset.z, "MuzzleOffset.z");

            TestPickup ammo = Check.NotNull(weapon.Ammo, "Ammo");
            Check.Equal("Energy Cell", ammo.PickupName, "Ammo.PickupName");
            Check.Equal(30, ammo.Quantity, "Ammo.Quantity");
        }

        [GameTest(GamePhases.Read)]
        public static void NoteAsset_LoadsSavedFields(GameTestContext context)
        {
            AssetObject? asset = context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name(NoteName));
            Check.NotNull(asset, $"Misc/{NoteName}");

            TestNoteAsset? note = asset as TestNoteAsset;
            Check.True(note != null, $"Misc/{NoteName} loaded as {asset!.GetType().Name}, not TestNoteAsset");
            Check.Equal("Remember the harness", note!.Text, "Text");
        }

        // manifests as an older or broken build of the game could have left them
        [GameTest]
        public static void StaleManifests_AreWritten(GameTestContext context)
        {
            WriteManifest(context, AssetBucket.Misc, "StaleNote", "TestNoteAsset StaleNote {\n    Text = \"still here\"\n    RemovedField = 5\n}\n");
            WriteManifest(context, AssetBucket.RawData, "PartialWeapon", "TestWeaponAsset PartialWeapon {\n    Damage = 7\n}\n");
            WriteManifest(context, AssetBucket.Misc, "GhostAsset", "RemovedAssetType GhostAsset {\n    Text = \"gone\"\n}\n");
            WriteManifest(context, AssetBucket.Misc, "NotAnAsset", "TestPickup NotAnAsset {\n    PickupName = \"not an asset\"\n}\n");
            WriteManifest(context, AssetBucket.Misc, "BrokenSyntax", "TestNoteAsset BrokenSyntax {\n    Text = = =\n");
            WriteManifest(context, AssetBucket.RawData, "WrongTypeWeapon", "TestWeaponAsset WrongTypeWeapon {\n    Damage = \"not a number\"\n    DisplayName = \"Wrong type\"\n}\n");
        }

        [GameTest(GamePhases.Read)]
        public static void StaleManifest_LoadsWithoutItsRemovedField(GameTestContext context)
        {
            TestNoteAsset note = Check.NotNull(context.AssetRegistry.GetAsset<TestNoteAsset>(AssetBucket.Misc, new Name("StaleNote")), "Misc/StaleNote");

            Check.Equal("still here", note.Text, "Text");
        }

        [GameTest(GamePhases.Read)]
        public static void PartialManifest_KeepsDefaultsForMissingFields(GameTestContext context)
        {
            TestWeaponAsset weapon = Check.NotNull(context.AssetRegistry.GetAsset<TestWeaponAsset>(AssetBucket.RawData, new Name("PartialWeapon")), "RawData/PartialWeapon");

            Check.Equal(7, weapon.Damage, "Damage");
            Check.Equal(0.0f, weapon.FireRate, "FireRate");
            Check.Equal(string.Empty, weapon.DisplayName, "DisplayName");
            Check.Equal(false, weapon.IsAutomatic, "IsAutomatic");
            Check.True(weapon.Ammo == null, "Ammo is not null");
        }

        [GameTest(GamePhases.Read)]
        public static void ManifestOfRemovedClass_DoesNotLoad(GameTestContext context)
        {
            Check.True(context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name("GhostAsset")) == null, "Misc/GhostAsset loaded though its class does not exist");
        }

        [GameTest(GamePhases.Read)]
        public static void ManifestOfNonAssetClass_DoesNotLoad(GameTestContext context)
        {
            Check.True(context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name("NotAnAsset")) == null, "Misc/NotAnAsset loaded though TestPickup is not an AssetObject");
        }

        [GameTest(GamePhases.Read)]
        public static void ManifestWithBrokenSyntax_DoesNotBreakTheRegistry(GameTestContext context)
        {
            // the parser recovers from syntax errors, so the asset may load with what could be read
            AssetObject? asset = context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name("BrokenSyntax"));

            if (asset != null)
            {
                Check.True(asset is TestNoteAsset, $"Misc/BrokenSyntax loaded as {asset.GetType().Name}");
            }

            Check.True(context.AssetRegistry.GetAsset(AssetBucket.Misc.Index, new Name(NoteName)) != null, $"Misc/{NoteName} no longer loads");
        }

        [GameTest(GamePhases.Read)]
        public static void ManifestWithWrongFieldType_DoesNotBreakTheRegistry(GameTestContext context)
        {
            AssetObject? asset = context.AssetRegistry.GetAsset(AssetBucket.RawData.Index, new Name("WrongTypeWeapon"));

            if (asset != null)
            {
                Check.True(asset is TestWeaponAsset, $"RawData/WrongTypeWeapon loaded as {asset.GetType().Name}");
            }

            // the registry still serves the assets around it
            Check.True(context.AssetRegistry.GetAsset(AssetBucket.RawData.Index, new Name(WeaponName)) != null, $"RawData/{WeaponName} no longer loads");
        }

        [GameTest(GamePhases.Read)]
        public static void UnloadableManifests_AreStillListed(GameTestContext context)
        {
            HashSet<string> miscNames = context.AssetRegistry.GetBucketAssetDescs(AssetBucket.Misc.Index).Select(desc => desc.Name.ToString()).ToHashSet();

            foreach (string expected in new[] { NoteName, "StaleNote", "GhostAsset", "NotAnAsset", "BrokenSyntax" })
            {
                Check.True(miscNames.Contains(expected), $"Misc does not list {expected} (lists: {string.Join(", ", miscNames)})");
            }
        }

        private static void WriteManifest(GameTestContext context, AssetBucket bucket, string name, string text)
        {
            string directory = Path.Combine(context.ContentDirectory, bucket.Name);

            Directory.CreateDirectory(directory);
            File.WriteAllText(Path.Combine(directory, name + ".hmf"), text);
        }

        private static TestWeaponAsset CreateWeapon(string name)
        {
            TestWeaponAsset weapon = new TestWeaponAsset
            {
                Damage = 42,
                FireRate = 2.5f,
                DisplayName = "Harness Blaster",
                IsAutomatic = true,
                MuzzleOffset = new Vec3f(1.0f, 2.0f, 3.0f),
                Ammo = new TestPickup { PickupName = "Energy Cell", Quantity = 30 }
            };

            weapon.SetName(new Name(name));

            return weapon;
        }

        private static TestNoteAsset CreateNote(string name)
        {
            TestNoteAsset note = new TestNoteAsset { Text = "Remember the harness" };
            note.SetName(new Name(name));

            return note;
        }

        private static void SaveAsset(GameTestContext context, AssetObject asset, AssetBucket expectedBucket)
        {
            // a project made in the editor starts out with its bucket folders; this one starts with none
            Directory.CreateDirectory(Path.Combine(context.ContentDirectory, expectedBucket.Name));

            context.AssetRegistry.PutAsset(asset);

            Check.Equal(expectedBucket.Index, asset.Path.BucketIndex, $"{asset.GetName()} bucket");
            Check.True(asset.Save().IsValid, $"Saving {asset.GetName()} failed, see the log");

            string manifestPath = Path.Combine(context.ContentDirectory, expectedBucket.Name, asset.GetName() + ".hmf");
            Check.True(File.Exists(manifestPath), $"{manifestPath} was not written");
        }
    }
}
