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
