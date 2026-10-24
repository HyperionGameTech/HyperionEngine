using Hyperion;

namespace GameTests
{
    public static class TypeTests
    {
        [GameTest]
        public static void GameObjectType_HasDynamicClass(GameTestContext context)
        {
            Check.True(Class.TryGetDynamicClass(typeof(TestPickup), out Class cls), "TestPickup has no dynamic class");
            Check.True(cls.IsValid, "TestPickup class is invalid");
            Check.Equal("TestPickup", cls.Name.ToString(), "TestPickup class name");
            Check.True(cls.GetProperty(new Name("PickupName")) != null, "TestPickup class has no PickupName property");
            Check.True(cls.GetProperty(new Name("Quantity")) != null, "TestPickup class has no Quantity property");
        }

        [GameTest]
        public static void GameObject_CreatesNativeInstance(GameTestContext context)
        {
            TestPickup pickup = new TestPickup { PickupName = "Probe", Quantity = 3 };

            Check.True(pickup.IsValid && pickup.NativeAddress != IntPtr.Zero, "TestPickup has no native object");
            Check.True(Class.TryGetDynamicClass(typeof(TestPickup), out Class cls) && pickup.Class == cls, "TestPickup instance is not of the TestPickup class");
        }

        [GameTest]
        public static void GameAssetType_DerivesFromAssetObject(GameTestContext context)
        {
            Check.True(Class.TryGetDynamicClass(typeof(TestWeaponAsset), out Class cls), "TestWeaponAsset has no dynamic class");
            Check.True(cls.IsSubclassOf(Class.GetClass(typeof(AssetObject))), "TestWeaponAsset class does not derive from AssetObject");

            ClassAttribute? bucketAttribute = cls.GetAttribute(new Name("assetbucket"));

            Check.True(bucketAttribute != null && bucketAttribute.Value.IsString, "TestWeaponAsset class has no assetbucket attribute");
            Check.Equal("RawData",bucketAttribute!.Value.GetString(), "TestWeaponAsset assetbucket attribute");
        }

        [GameTest]
        public static void UnboundAssetType_HasDynamicClass(GameTestContext context)
        {
            Check.True(Class.TryGetDynamicClass(typeof(TestNoteAsset), out Class cls), "TestNoteAsset has no dynamic class");
            Check.True(cls.IsSubclassOf(Class.GetClass(typeof(AssetObject))), "TestNoteAsset class does not derive from AssetObject");
            Check.True(cls.GetProperty(new Name("Text")) != null, "TestNoteAsset class has no Text property");
        }
    }
}
