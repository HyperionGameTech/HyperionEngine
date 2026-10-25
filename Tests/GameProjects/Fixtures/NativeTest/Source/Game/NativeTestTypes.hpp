#pragma once

#include <Asset/AssetObject.hpp>
#include <Asset/AssetReference.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

// Game types reflected by CodeGen, which also writes their C# bindings (NativeTestGame.Bindings.dll)

HYP_ENUM()
enum class NativeTestRarity : uint8
{
    Common = 0,
    Rare = 2,
    Legendary
};

HYP_CLASS()
class NativeTestPickup : public ObjectBase
{
    HYP_OBJECT_BODY(NativeTestPickup);

public:
    HYP_FIELD(Property = "PickupName", Serialize)
    String pickupName;

    HYP_FIELD(Property = "Quantity", Serialize)
    int32 quantity = 0;

    HYP_METHOD()
    int32 GetDoubledQuantity() const
    {
        return quantity * 2;
    }
};

HYP_CLASS(AssetBucket = "RawData")
class NativeTestWeaponAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestWeaponAsset);

public:
    HYP_FIELD(Property = "Damage", Serialize)
    int32 damage = 0;

    HYP_FIELD(Property = "FireRate", Serialize)
    float fireRate = 0.0f;

    HYP_FIELD(Property = "DisplayName", Serialize)
    String displayName;

    HYP_FIELD(Property = "IsAutomatic", Serialize)
    bool isAutomatic = false;

    HYP_FIELD(Property = "MuzzleOffset", Serialize)
    Vec3f muzzleOffset = Vec3f::Zero();

    HYP_FIELD(Property = "Rarity", Serialize)
    NativeTestRarity rarity = NativeTestRarity::Common;

    HYP_FIELD(Property = "Ammo", Serialize)
    Handle<NativeTestPickup> ammo;
};

// no AssetBucket attribute, so the registry puts it in the Misc bucket
HYP_CLASS()
class NativeTestNoteAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestNoteAsset);

public:
    HYP_FIELD(Property = "Text", Serialize)
    String text;
};

HYP_CLASS(AssetBucket = "Decals")
class NativeTestGearAsset : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestGearAsset);

public:
    HYP_FIELD(Property = "Weight", Serialize)
    float weight = 0.0f;
};

// takes its bucket and weight from NativeTestGearAsset
HYP_CLASS()
class NativeTestArmorAsset final : public NativeTestGearAsset
{
    HYP_OBJECT_BODY(NativeTestArmorAsset);

public:
    HYP_FIELD(Property = "Defense", Serialize)
    int32 defense = 0;
};

HYP_CLASS(AssetBucket = "RawData", NoScriptBindings)
class NativeTestLoadoutAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestLoadoutAsset);

public:
    HYP_FIELD(Property = "Tag", Serialize)
    Name tag;

    HYP_FIELD(Property = "Serial", Serialize)
    int64 serial = 0;

    HYP_FIELD(Property = "Mass", Serialize)
    double mass = 0.0;

    HYP_FIELD(Property = "Slot", Serialize)
    uint8 slot = 0;

    HYP_FIELD(Property = "Rarity", Serialize)
    NativeTestRarity rarity = NativeTestRarity::Common;

    HYP_FIELD(Property = "AmmoCounts", Serialize)
    Array<int32> ammoCounts;

    HYP_FIELD(Property = "Labels", Serialize)
    Array<String> labels;

    HYP_FIELD(Property = "Pickups", Serialize)
    Array<Handle<NativeTestPickup>> pickups;

    // other assets are saved as references to them
    HYP_FIELD(Property = "Note", Serialize)
    Handle<NativeTestNoteAsset> note;

    HYP_FIELD(Property = "Primary", Serialize)
    Handle<NativeTestWeaponAsset> primary;

    HYP_FIELD(Property = "ExtraReference", Serialize)
    AssetReference extraReference;

    HYP_FIELD(Property = "Scratch", Transient)
    int32 scratch = 7;
};

// members matched by name across classes, as a reloaded game class is matched to its previous build
HYP_CLASS()
class NativeTestCounterBase : public ObjectBase
{
    HYP_OBJECT_BODY(NativeTestCounterBase);

public:
    HYP_FIELD(Property = "BaseValue", Serialize)
    int32 baseValue = 0;
};

HYP_CLASS()
class NativeTestCounter final : public NativeTestCounterBase
{
    HYP_OBJECT_BODY(NativeTestCounter);

public:
    HYP_FIELD(Property = "Count", Serialize)
    int32 count = 0;

    HYP_FIELD(Property = "Label", Serialize)
    String label;

    HYP_FIELD(Property = "Scratch", Transient)
    int32 scratch = 0;
};

HYP_CLASS()
class NativeTestWideCounter final : public NativeTestCounterBase
{
    HYP_OBJECT_BODY(NativeTestWideCounter);

public:
    HYP_FIELD(Property = "Count", Serialize)
    double count = 0.0;

    HYP_FIELD(Property = "Label", Serialize)
    String label;

    HYP_FIELD(Property = "Scratch", Transient)
    int32 scratch = 0;
};

} // namespace Hyperion
