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
