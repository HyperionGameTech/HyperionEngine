using Hyperion;

[ClassBinding(IsDynamic = true)]
public class TestPickup : ObjectBase
{
    [HypField(Serialize = true)]
    public string PickupName = string.Empty;

    [HypField(Serialize = true)]
    public int Quantity = 0;
}

[ClassBinding(IsDynamic = true)]
[AssetBucket("RawData")]
public class TestWeaponAsset : AssetObject
{
    [HypField(Serialize = true)]
    public int Damage = 0;

    [HypField(Serialize = true)]
    public float FireRate = 0.0f;

    [HypField(Serialize = true)]
    public string DisplayName = string.Empty;

    [HypField(Serialize = true)]
    public bool IsAutomatic = false;

    [HypField(Serialize = true)]
    public Vec3f MuzzleOffset = Vec3f.Zero;

    [HypField(Serialize = true)]
    public TestPickup? Ammo = null;
}

// No ClassBinding or AssetBucket: gets its class from the [HypField] members alone, and lands in the Misc bucket
public class TestNoteAsset : AssetObject
{
    [HypField(Serialize = true)]
    public string Text = string.Empty;
}

public enum TestRarity : byte
{
    Common = 0,
    Rare = 2,
    Legendary
}

[ClassBinding(IsDynamic = true)]
[AssetBucket("Decals")]
public class TestGearAsset : AssetObject
{
    [HypField(Serialize = true)]
    public float Weight = 0.0f;
}

// takes its bucket and Weight from TestGearAsset
[ClassBinding(IsDynamic = true)]
public class TestArmorAsset : TestGearAsset
{
    [HypField(Serialize = true)]
    public int Defense = 0;
}

[ClassBinding(IsDynamic = true)]
[AssetBucket("RawData")]
public class TestLoadoutAsset : AssetObject
{
    [HypField(Serialize = true)]
    public Name Tag;

    [HypField(Serialize = true)]
    public long Serial = 0;

    [HypField(Serialize = true)]
    public double Mass = 0.0;

    [HypField(Serialize = true)]
    public byte Slot = 0;

    [HypField(Serialize = true)]
    public TestRarity Rarity = TestRarity.Common;

    // other assets are saved as references to them
    [HypField(Serialize = true)]
    public TestNoteAsset? Note = null;

    [HypField(Serialize = true)]
    public TestWeaponAsset? Primary = null;

    [HypField(Serialize = true)]
    public TestPickup? Spare = null;

    [HypField(Transient = true)]
    public int Scratch = 7;
}
