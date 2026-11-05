/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorIsolatedEditState.hpp>

#include <Scene/Instancing/PrefabSet.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <ArborEditor/ArborShared.hpp>

namespace Hyperion {

class EditorSubsystem;
class Entity;
class Node;
class Prefab;

enum class ArborWorkFlags : uint32
{
    None = 0x0,
    RegrowPending = 0x1,
    Growing = 0x2,
    BakingVariants = 0x4,
    BakeRequested = 0x8
};

HYP_MAKE_ENUM_FLAGS(ArborWorkFlags);

HYP_CLASS(Serialize = false)
class ARBOREDITOR_API EditorArborState final : public EditorIsolatedEditState
{
    HYP_OBJECT_BODY(EditorArborState);

public:
    EditorArborState();
    ~EditorArborState() override;

    void Initialize(EditorSubsystem* subsystem);

    bool IsActive() const override;

    bool CanEditAsset(const Handle<AssetObject>& asset) const override;
    bool EnterAsset(const Handle<AssetObject>& asset) override;
    Handle<AssetObject> GetInspectedAsset() const override;

    bool Apply() override;
    void Revert() override;

    String GetDisplayName() const override;
    String GetKindName() const override;

    HYP_METHOD()
    bool IsAvailable() const;

    HYP_METHOD()
    uint32 GetPresetCount(uint32 kind) const;

    HYP_METHOD()
    String GetPresetName(uint32 kind, uint32 index) const;

    HYP_METHOD()
    bool EnterNewPlant(uint32 kind, const String& preset);

    HYP_METHOD()
    bool EnterPrefab(const Handle<Prefab>& prefab);

    HYP_METHOD()
    bool EnterPrefabSet(const Handle<PrefabSet>& prefabSet);

    HYP_METHOD()
    uint32 GetVariantCount() const;

    HYP_METHOD()
    void SetVariantCount(uint32 variantCount);

    HYP_METHOD()
    bool CanSetVariantCount() const;

    HYP_METHOD()
    bool HasTarget() const;

    HYP_METHOD()
    String GetTargetName() const;

    HYP_METHOD()
    uint32 GetTargetKind() const;

    HYP_METHOD()
    String GetRecipeName() const;

    HYP_METHOD()
    String GetRecipePreset() const;

    HYP_METHOD()
    void ApplyPreset(const String& preset);

    HYP_METHOD()
    uint32 GetSeed() const;

    HYP_METHOD()
    void SetSeed(uint32 seed);

    HYP_METHOD()
    void RandomizeSeed();

    HYP_METHOD()
    uint32 GetLayoutVersion() const;

    HYP_METHOD()
    uint32 GetControlCount() const;

    HYP_METHOD()
    String GetControlId(uint32 index) const;

    HYP_METHOD()
    String GetControlSection(uint32 index) const;

    HYP_METHOD()
    String GetControlGroup(uint32 index) const;

    HYP_METHOD()
    String GetControlLabel(uint32 index) const;

    HYP_METHOD()
    String GetControlHelp(uint32 index) const;

    HYP_METHOD()
    uint32 GetControlKind(uint32 index) const;

    HYP_METHOD()
    float GetControlMin(uint32 index) const;

    HYP_METHOD()
    float GetControlMax(uint32 index) const;

    HYP_METHOD()
    bool IsControlLog(uint32 index) const;

    HYP_METHOD()
    uint32 GetControlCountMin(uint32 index) const;

    HYP_METHOD()
    uint32 GetControlCountMax(uint32 index) const;

    HYP_METHOD()
    uint32 GetControlOptionCount(uint32 index) const;

    HYP_METHOD()
    String GetControlOption(uint32 index, uint32 option) const;

    HYP_METHOD()
    bool IsControlRange(uint32 index) const;

    HYP_METHOD()
    float GetControlLo(uint32 index) const;

    HYP_METHOD()
    float GetControlHi(uint32 index) const;

    HYP_METHOD()
    float GetControlLanded(uint32 index) const;

    HYP_METHOD()
    uint32 GetControlInteger(uint32 index) const;

    HYP_METHOD()
    void SetControlRanged(uint32 index, bool isRange, float lo, float hi);

    HYP_METHOD()
    void SetControlInteger(uint32 index, uint32 value);

    HYP_METHOD()
    void CommitEdit();

    HYP_METHOD()
    bool IsGenerating() const;

    HYP_METHOD()
    bool IsNewPlant() const;

    HYP_METHOD()
    bool CanBake() const;

    HYP_METHOD()
    void Bake();

    HYP_METHOD()
    String GetStatusText() const;

    HYP_METHOD()
    String GetRecipeRon() const;

    HYP_METHOD()
    bool SetRecipeRon(const String& ron);

    HYP_METHOD()
    int32 FindControl(const String& id) const;

    HYP_METHOD()
    bool SetControlRangedById(const String& id, bool isRange, float lo, float hi);

    HYP_METHOD()
    bool SetControlIntegerById(const String& id, uint32 value);

    HYP_METHOD()
    String DescribeControls() const;

    HYP_METHOD()
    uint32 GetPlantKindCount() const;

    HYP_METHOD()
    String GetPlantKindName(uint32 kind) const;

    HYP_METHOD()
    String GetLastBakedAssetPath() const;

    HYP_METHOD()
    Handle<AssetObject> GetLastBakedAsset() const;

    void Update();

    static TResult<Handle<Prefab>> GrowPrefab(const ArborRecipeHandle& recipe);

    bool GrowNow();

    HYP_FIELD()
    ScriptableDelegate<void> OnStateChanged;

protected:
    void ClearTarget() override;

private:
    struct GrownPlant
    {
        Handle<Prefab> prefab;
        String error;
        double milliseconds = 0.0;

        void Take(TResult<Handle<Prefab>>&& result)
        {
            if (result.HasError())
            {
                error = result.GetError().GetMessage();
            }
            else
            {
                prefab = std::move(result.GetValue());
            }
        }
    };

    bool EnterPlant(const Handle<Entity>& plant, ArborKind kind, ArborRecipeHandle&& recipe, Name sceneName);
    void RebuildControls();

    void RecipeChanged(bool reshaped);
    bool SetControlValue(uint32 index, const ArborControlValue& value);
    void RestoreRecipe(const String& ron);

    Handle<Prefab> BakeNewPrefab(const Handle<Node>& root);

    bool BakesVariants() const;

    Handle<Node> CloneGrownPlant() const;
    Handle<Node> MakeVariantRoot(Name name, const ArborRecipeHandle& recipe, const Handle<Node>& grownNode) const;
    Array<Handle<Node>> CollectVariantNodes(Span<const Handle<Prefab>> grown) const;

    bool BakeVariantSet(const Array<Handle<Node>>& grownNodes);
    void StartBakingVariants();
    void FinishBakingVariants(Array<Handle<Prefab>>&& grown, const String& error, const String& bakedRecipe);

    String MakeBakeFolder(Name name) const;
    void FileAssetsDeep(const Handle<AssetObject>& asset, const String& folder);

    void StartGrowing(ArborRecipeHandle&& recipe);
    void FinishGrowing(const Handle<Entity>& plant, GrownPlant&& grown);

    Handle<Entity> m_plant;
    Handle<Prefab> m_sourcePrefab;
    Handle<PrefabSet> m_sourceSet;

    Handle<AssetObject> m_lastBaked;

    uint32 m_variantCount = 1;

    ArborRecipeHandle m_recipe;
    ArborKind m_kind = ArborKind::Tree;

    Array<ArborControlDesc> m_controls;
    uint32 m_layoutVersion = 0;

    String m_appliedRecipe;
    String m_committedRecipe;

    EnumFlags<ArborWorkFlags> m_work = ArborWorkFlags::None;
    ClockTimer m_regrowTimer;

    String m_statusText;

    mutable ArborKind m_presetLookupKind = ArborKind::Max;
    mutable String m_presetLookupName;
    mutable String m_presetLookupResult;
};

} // namespace Hyperion
