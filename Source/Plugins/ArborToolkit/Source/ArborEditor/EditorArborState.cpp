/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <ArborEditor/EditorArborState.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorAssetFolders.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/Prefab.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/ModelLoaders/GLTFModelLoader.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/Utilities/Time.hpp>

#include <EditorArborState.generated.inl>

namespace Hyperion {

ARBOREDITOR_API HYP_DECLARE_LOG_CHANNEL(Arbor);

namespace {

constexpr ClockTimer::TickUnit RegrowDelay = 0.08f;

const Name g_nameArborKind = NAME("ArborKind");
const Name g_nameArborRecipe = NAME("ArborRecipe");

void WriteRecipe(Node* plant, ArborKind kind, const String& ron)
{
    plant->RemoveDataEntry(g_nameArborKind);
    plant->RemoveDataEntry(g_nameArborRecipe);

    plant->AddDataEntry(NodeData(g_nameArborKind, int(kind)));
    plant->AddDataEntry(NodeData(g_nameArborRecipe, ron));
}

bool ReadRecipe(const Node* plant, ArborKind& outKind, String& outRon)
{
    const int* kind = plant->GetDataEntry(g_nameArborKind).data.TryGet<int>();
    const String* ron = plant->GetDataEntry(g_nameArborRecipe).data.TryGet<String>();

    if (!kind || !ron || *kind < 0 || *kind >= int(ArborKind::Max))
    {
        return false;
    }

    outKind = ArborKind(*kind);
    outRon = *ron;

    return true;
}

void SetGrown(const Node* plant, bool generated)
{
    for (const Handle<Node>& child : plant->GetChildren())
    {
        if (child.IsValid())
        {
            child->SetNodeFlags(generated
                    ? child->GetNodeFlags() | NodeFlags::Generated
                    : child->GetNodeFlags() & ~NodeFlags::Generated);
        }
    }
}

bool IsArborPrefab(const Prefab* prefab)
{
    return prefab && prefab->GetRoot().IsValid() && prefab->GetRoot()->HasDataEntry(g_nameArborRecipe);
}

bool IsGrownNode(const Handle<Node>& node)
{
    return node.IsValid() && (node->GetNodeFlags() & NodeFlags::Generated);
}

Handle<Node> FindGrownChild(const Node* plant)
{
    for (const Handle<Node>& child : plant->GetChildren())
    {
        if (IsGrownNode(child))
        {
            return child;
        }
    }

    return Handle<Node>::Null();
}

Handle<Node> CloneUntagged(const Node* node)
{
    Handle<Node> clone = node->Clone();
    Prefab::UntagAsPrefabInstance(clone.Get());

    return clone;
}

ArborRecipeHandle MakeVariantRecipe(
    const ArborRecipeHandle& recipe,
    uint32 index)
{
    ArborRecipeHandle variant = recipe.Clone();
    variant.SetSeed(recipe.GetSeed() + index);

    return variant;
}

TResult<Array<Handle<Prefab>>> GrowVariants(
    const ArborRecipeHandle& recipe,
    uint32 variantCount)
{
    Array<Handle<Prefab>> grown;

    for (uint32 index = 1; index < variantCount; index++)
    {
        TResult<Handle<Prefab>> prefab = EditorArborState::GrowPrefab(MakeVariantRecipe(recipe, index));

        if (prefab.HasError())
        {
            return HYP_MAKE_ERROR(Error, "variant {}: {}", index + 1, prefab.GetError().GetMessage());
        }

        grown.PushBack(std::move(prefab.GetValue()));
    }

    return grown;
}

template <class Container, class Predicate>
bool AllOf(const Container& container, Predicate&& predicate)
{
    for (const auto& item : container)
    {
        if (!predicate(item))
        {
            return false;
        }
    }

    return true;
}

constexpr uint32 MaxVariants = 32;

} // namespace

EditorArborState::EditorArborState() = default;

EditorArborState::~EditorArborState() = default;

void EditorArborState::Initialize(EditorSubsystem* subsystem)
{
    EditorIsolatedEditState::Initialize(subsystem);

    if (!ArborRecipeHandle::IsAvailable())
    {
        m_statusText = ArborRecipeHandle::GetLastError();
    }
}

bool EditorArborState::IsActive() const
{
    return m_plant.IsValid() && m_editScene.IsValid();
}

bool EditorArborState::CanEditAsset(const Handle<AssetObject>& asset) const
{
    if (!asset.IsValid())
    {
        return false;
    }

    if (const Prefab* prefab = TryCast<Prefab>(asset.Get()))
    {
        return IsArborPrefab(prefab);
    }

    if (const PrefabSet* prefabSet = TryCast<PrefabSet>(asset.Get()))
    {
        const Array<PrefabSetEntry> entries = prefabSet->GetValidEntries();

        return entries.Any() && AllOf(entries, [](const PrefabSetEntry& entry) { return IsArborPrefab(entry.prefab.Get()); });
    }

    return false;
}

bool EditorArborState::EnterAsset(const Handle<AssetObject>& asset)
{
    if (!CanEditAsset(asset))
    {
        return false;
    }

    if (Handle<PrefabSet> prefabSet = TryCast<PrefabSet>(asset); prefabSet.IsValid())
    {
        return EnterPrefabSet(prefabSet);
    }

    return EnterPrefab(TryCast<Prefab>(asset));
}

Handle<AssetObject> EditorArborState::GetInspectedAsset() const
{
    if (m_sourceSet.IsValid())
    {
        return m_sourceSet;
    }

    return m_sourcePrefab;
}

String EditorArborState::GetDisplayName() const
{
    if (m_sourceSet.IsValid())
    {
        return String(*m_sourceSet->GetName());
    }

    return m_sourcePrefab.IsValid() ? String(*m_sourcePrefab->GetName()) : m_recipe.GetName();
}

String EditorArborState::GetKindName() const
{
    return "Arbor plant";
}

bool EditorArborState::IsAvailable() const
{
    return ArborRecipeHandle::IsAvailable();
}

uint32 EditorArborState::GetPresetCount(uint32 kind) const
{
    return ArborRecipeHandle::GetPresetCount(kind);
}

String EditorArborState::GetPresetName(uint32 kind, uint32 index) const
{
    return ArborRecipeHandle::GetPresetName(kind, index);
}

bool EditorArborState::EnterNewPlant(uint32 kind, const String& preset)
{
    AssertOnThread(g_simThread);

    if (kind >= uint32(ArborKind::Max) || !m_subsystem)
    {
        return false;
    }

    ArborRecipeHandle recipe = ArborRecipeHandle::FromPreset(kind, preset);

    if (!recipe.IsValid())
    {
        HYP_LOG(Arbor, Warning, "cannot grow preset {}: {}", preset, ArborRecipeHandle::GetLastError());

        return false;
    }

    Handle<Entity> plant = MakeHandle<Entity>(CreateNameFromDynamicString(preset.ToAnsi()));
    WriteRecipe(plant.Get(), ArborKind(kind), recipe.ToRon());

    if (!EnterPlant(plant, ArborKind(kind), std::move(recipe), NAME_FMT("ArborEdit_{}", preset)))
    {
        return false;
    }

    SetDirty(true);

    return true;
}

bool EditorArborState::EnterPrefab(const Handle<Prefab>& prefab)
{
    AssertOnThread(g_simThread);

    if (!CanEditAsset(prefab))
    {
        return false;
    }

    Handle<Entity> plant = TryCast<Entity>(CloneUntagged(prefab->GetRoot().Get()));

    if (!plant.IsValid())
    {
        return false;
    }

    plant->SetName(prefab->GetName());

    ArborKind kind = ArborKind::Tree;
    String ron;

    if (!ReadRecipe(plant.Get(), kind, ron))
    {
        HYP_LOG(Arbor, Warning, "prefab {} has no readable Arbor recipe", prefab->GetName());

        return false;
    }

    ArborRecipeHandle recipe = ArborRecipeHandle::FromRon(uint32(kind), ron);

    if (!recipe.IsValid())
    {
        HYP_LOG(Arbor, Warning, "cannot read the recipe of {}: {}", prefab->GetName(), ArborRecipeHandle::GetLastError());

        return false;
    }

    SetGrown(plant.Get(), /* generated */ true);

    if (!EnterPlant(plant, kind, std::move(recipe), NAME_FMT("ArborEdit_{}", prefab->GetName())))
    {
        return false;
    }

    m_sourcePrefab = prefab;
    m_work &= ~ArborWorkFlags::RegrowPending;

    OnStateChanged();

    return true;
}

bool EditorArborState::EnterPrefabSet(const Handle<PrefabSet>& prefabSet)
{
    AssertOnThread(g_simThread);

    if (!CanEditAsset(prefabSet))
    {
        return false;
    }

    const Array<PrefabSetEntry> entries = prefabSet->GetValidEntries();

    if (!EnterPrefab(entries[0].prefab))
    {
        return false;
    }

    m_sourcePrefab.Reset();
    m_sourceSet = prefabSet;
    m_variantCount = uint32(entries.Size());

    OnStateChanged();

    return true;
}

uint32 EditorArborState::GetVariantCount() const
{
    return m_variantCount;
}

void EditorArborState::SetVariantCount(uint32 variantCount)
{
    if (!CanSetVariantCount())
    {
        return;
    }

    m_variantCount = MathUtil::Clamp(variantCount, 1u, MaxVariants);

    OnStateChanged();
}

bool EditorArborState::CanSetVariantCount() const
{
    return IsNewPlant() || m_sourceSet.IsValid();
}

bool EditorArborState::EnterPlant(const Handle<Entity>& plant, ArborKind kind, ArborRecipeHandle&& recipe, Name sceneName)
{
    if (!BeginEnter())
    {
        return false;
    }

    CreateScenes(sceneName);

    InitObject(plant);
    m_editScene->SetRoot(plant);

    m_plant = plant;
    m_kind = kind;
    m_recipe = std::move(recipe);
    m_appliedRecipe = m_recipe.ToRon();
    m_committedRecipe = m_appliedRecipe;
    m_work &= ~ArborWorkFlags::BakeRequested;
    m_work |= ArborWorkFlags::RegrowPending;

    RebuildControls();

    FinishEnter();

    OnStateChanged();

    return true;
}

void EditorArborState::ClearTarget()
{
    m_plant.Reset();
    m_sourcePrefab.Reset();
    m_sourceSet.Reset();
    m_variantCount = 1;
    m_work &= ~(ArborWorkFlags::RegrowPending | ArborWorkFlags::BakingVariants | ArborWorkFlags::BakeRequested);
    m_recipe = ArborRecipeHandle();
    m_controls.Clear();
    m_layoutVersion++;
    m_appliedRecipe = String::empty;
    m_committedRecipe = String::empty;

    OnStateChanged();
}

bool EditorArborState::HasTarget() const
{
    return IsActive() && m_recipe.IsValid();
}

String EditorArborState::GetTargetName() const
{
    return m_plant.IsValid() ? String(m_plant->GetName().LookupString()) : String::empty;
}

uint32 EditorArborState::GetTargetKind() const
{
    return uint32(m_kind);
}

String EditorArborState::GetRecipeName() const
{
    return m_recipe.GetName();
}

String EditorArborState::GetRecipePreset() const
{
    const String name = m_recipe.GetName();

    if (m_presetLookupKind == m_kind && m_presetLookupName == name)
    {
        return m_presetLookupResult;
    }

    m_presetLookupKind = m_kind;
    m_presetLookupName = name;
    m_presetLookupResult = String::empty;

    const uint32 count = ArborRecipeHandle::GetPresetCount(uint32(m_kind));

    for (uint32 index = 0; index < count && !name.Empty(); index++)
    {
        const String preset = ArborRecipeHandle::GetPresetName(uint32(m_kind), index);

        if (ArborRecipeHandle::FromPreset(uint32(m_kind), preset).GetName() == name)
        {
            m_presetLookupResult = preset;
            break;
        }
    }

    return m_presetLookupResult;
}

void EditorArborState::ApplyPreset(const String& preset)
{
    ArborRecipeHandle recipe = ArborRecipeHandle::FromPreset(uint32(m_kind), preset);

    if (!recipe.IsValid() || !HasTarget())
    {
        m_statusText = ArborRecipeHandle::GetLastError();
        OnStateChanged();

        return;
    }

    recipe.SetSeed(m_recipe.GetSeed());

    m_recipe = std::move(recipe);
    RecipeChanged(/* reshaped */ true);
    CommitEdit();
}

uint32 EditorArborState::GetSeed() const
{
    return uint32(m_recipe.GetSeed());
}

void EditorArborState::SetSeed(uint32 seed)
{
    if (!HasTarget() || m_recipe.GetSeed() == seed)
    {
        return;
    }

    m_recipe.SetSeed(seed);
    RecipeChanged(/* reshaped */ false);
    CommitEdit();
}

void EditorArborState::RandomizeSeed()
{
    const uint64 ticks = Time::Now().ToMilliseconds();

    SetSeed(uint32((ticks * 0x9E3779B97F4A7C15ull) >> 40));
}

uint32 EditorArborState::GetLayoutVersion() const
{
    return m_layoutVersion;
}

uint32 EditorArborState::GetControlCount() const
{
    return uint32(m_controls.Size());
}

String EditorArborState::GetControlId(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].id : String::empty;
}

String EditorArborState::GetControlSection(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].section : String::empty;
}

String EditorArborState::GetControlGroup(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].group : String::empty;
}

String EditorArborState::GetControlLabel(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].label : String::empty;
}

String EditorArborState::GetControlHelp(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].help : String::empty;
}

uint32 EditorArborState::GetControlKind(uint32 index) const
{
    return index < m_controls.Size() ? uint32(m_controls[index].kind) : 0;
}

float EditorArborState::GetControlMin(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].min : 0.0f;
}

float EditorArborState::GetControlMax(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].max : 1.0f;
}

bool EditorArborState::IsControlLog(uint32 index) const
{
    return index < m_controls.Size() && m_controls[index].log;
}

uint32 EditorArborState::GetControlCountMin(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].countMin : 0;
}

uint32 EditorArborState::GetControlCountMax(uint32 index) const
{
    return index < m_controls.Size() ? m_controls[index].countMax : 0;
}

uint32 EditorArborState::GetControlOptionCount(uint32 index) const
{
    return index < m_controls.Size() ? uint32(m_controls[index].options.Size()) : 0;
}

String EditorArborState::GetControlOption(uint32 index, uint32 option) const
{
    if (index >= m_controls.Size() || option >= m_controls[index].options.Size())
    {
        return String::empty;
    }

    return m_controls[index].options[option];
}

bool EditorArborState::IsControlRange(uint32 index) const
{
    ArborControlValue value;

    return m_recipe.GetValue(index, value) && value.isRange;
}

float EditorArborState::GetControlLo(uint32 index) const
{
    ArborControlValue value;

    return m_recipe.GetValue(index, value) ? value.lo : 0.0f;
}

float EditorArborState::GetControlHi(uint32 index) const
{
    ArborControlValue value;

    return m_recipe.GetValue(index, value) ? value.hi : 0.0f;
}

float EditorArborState::GetControlLanded(uint32 index) const
{
    ArborControlValue value;

    return m_recipe.GetValue(index, value) ? value.landed : 0.0f;
}

uint32 EditorArborState::GetControlInteger(uint32 index) const
{
    ArborControlValue value;

    return m_recipe.GetValue(index, value) ? value.integer : 0;
}

bool EditorArborState::SetControlValue(uint32 index, const ArborControlValue& value)
{
    bool reshaped = false;

    if (!HasTarget() || !m_recipe.SetValue(index, value, reshaped))
    {
        return false;
    }

    RecipeChanged(reshaped);

    return true;
}

void EditorArborState::SetControlRanged(uint32 index, bool isRange, float lo, float hi)
{
    ArborControlValue value;
    value.isRange = isRange;
    value.lo = lo;
    value.hi = isRange ? hi : lo;

    SetControlValue(index, value);
}

void EditorArborState::SetControlInteger(uint32 index, uint32 integer)
{
    ArborControlValue value;
    value.integer = integer;

    SetControlValue(index, value);
}

String EditorArborState::GetRecipeRon() const
{
    return HasTarget() ? m_recipe.ToRon() : String::empty;
}

bool EditorArborState::SetRecipeRon(const String& ron)
{
    if (!HasTarget())
    {
        return false;
    }

    ArborRecipeHandle recipe = ArborRecipeHandle::FromRon(uint32(m_kind), ron);

    if (!recipe.IsValid())
    {
        m_statusText = HYP_FORMAT("Not a {} recipe: {}", GetPlantKindName(uint32(m_kind)), ArborRecipeHandle::GetLastError());
        OnStateChanged();

        return false;
    }

    m_recipe = std::move(recipe);
    RecipeChanged(/* reshaped */ true);
    CommitEdit();

    return true;
}

int32 EditorArborState::FindControl(const String& id) const
{
    for (uint32 index = 0; index < m_controls.Size(); index++)
    {
        if (m_controls[index].id == id)
        {
            return int32(index);
        }
    }

    return -1;
}

bool EditorArborState::SetControlRangedById(const String& id, bool isRange, float lo, float hi)
{
    const int32 index = FindControl(id);

    if (index < 0 || m_controls[index].kind != ArborControlKind::Ranged)
    {
        return false;
    }

    ArborControlValue value;
    value.isRange = isRange;
    value.lo = lo;
    value.hi = isRange ? hi : lo;

    if (!SetControlValue(uint32(index), value))
    {
        return false;
    }

    // the panel commits when a drag ends; a call by id is the whole edit
    CommitEdit();

    return true;
}

bool EditorArborState::SetControlIntegerById(const String& id, uint32 integer)
{
    const int32 index = FindControl(id);

    if (index < 0 || m_controls[index].kind == ArborControlKind::Ranged)
    {
        return false;
    }

    ArborControlValue value;
    value.integer = integer;

    if (!SetControlValue(uint32(index), value))
    {
        return false;
    }

    CommitEdit();

    return true;
}

String EditorArborState::DescribeControls() const
{
    constexpr ANSIStringView Names[] = {
        "ranged", "count", "choice", "toggle"
    };

    JSON::JArray controls;

    for (uint32 index = 0; index < m_controls.Size(); index++)
    {
        const ArborControlDesc& desc = m_controls[index];

        JSON::Object control;
        control.Set("index", JSON::Value(double(index)));
        control.Set("id", JSON::Value(desc.id));
        control.Set("section", JSON::Value(desc.section));
        control.Set("group", JSON::Value(desc.group));
        control.Set("label", JSON::Value(desc.label));
        control.Set("help", JSON::Value(desc.help));
        control.Set("kind", JSON::Value(String(Names[MathUtil::Min(uint32(desc.kind), 3u)])));

        ArborControlValue value;
        const bool hasValue = m_recipe.GetValue(index, value);

        if (desc.kind == ArborControlKind::Ranged)
        {
            control.Set("min", JSON::Value(double(desc.min)));
            control.Set("max", JSON::Value(double(desc.max)));
            control.Set("log", JSON::Value(desc.log));

            if (hasValue)
            {
                control.Set("isRange", JSON::Value(value.isRange));
                control.Set("lo", JSON::Value(double(value.lo)));
                control.Set("hi", JSON::Value(double(value.hi)));
                // where this seed lands in the range
                control.Set("landed", JSON::Value(double(value.landed)));
            }
        }
        else
        {
            if (desc.kind == ArborControlKind::Count)
            {
                control.Set("min", JSON::Value(double(desc.countMin)));
                control.Set("max", JSON::Value(double(desc.countMax)));
            }

            if (desc.kind == ArborControlKind::Choice)
            {
                JSON::JArray options;

                for (const String& option : desc.options)
                {
                    options.PushBack(JSON::Value(option));
                }

                control.Set("options", JSON::Value(std::move(options)));
            }

            if (hasValue)
            {
                control.Set("value", JSON::Value(double(value.integer)));
            }
        }

        control.Set("regrows", JSON::Value(desc.regrows));

        controls.PushBack(JSON::Value(std::move(control)));
    }

    JSON::Object result;
    result.Set("layoutVersion", JSON::Value(double(m_layoutVersion)));
    result.Set("controls", JSON::Value(std::move(controls)));

    return String(JSON::Value(std::move(result)).ToString());
}

uint32 EditorArborState::GetPlantKindCount() const
{
    return uint32(ArborKind::Max);
}

String EditorArborState::GetPlantKindName(uint32 kind) const
{
    switch (ArborKind(kind))
    {
    case ArborKind::Tree:
        return "Tree";
    case ArborKind::Cover:
        return "Cover";
    case ArborKind::Rock:
        return "Rock";
    default:
        return String::empty;
    }
}

String EditorArborState::GetLastBakedAssetPath() const
{
    if (!m_lastBaked.IsValid() || !m_lastBaked->IsRegistered())
    {
        return String::empty;
    }

    return HYP_FORMAT("{}/{}", m_lastBaked->GetPath().GetBucket().GetName(), m_lastBaked->GetName());
}

Handle<AssetObject> EditorArborState::GetLastBakedAsset() const
{
    return m_lastBaked;
}

void EditorArborState::CommitEdit()
{
    if (!HasTarget())
    {
        return;
    }

    const String after = m_recipe.ToRon();

    if (after == m_committedRecipe)
    {
        return;
    }

    const String before = m_committedRecipe;
    m_committedRecipe = after;

    const WeakHandle<EditorArborState> weakThis = WeakHandleFromThis();
    const WeakHandle<Entity> weakPlant = MakeWeakRef(m_plant);

    auto restore = [weakThis, weakPlant](const String& ron)
    {
        Handle<EditorArborState> state = weakThis.Lock();

        if (!state.IsValid()
            || !state->IsActive()
            || state->m_plant != weakPlant.Lock()
            || state->m_recipe.ToRon() == ron)
        {
            return;
        }

        state->RestoreRecipe(ron);
    };

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        "Edit Arbor Plant",
        Proc<EditorActionFunctions()>(
            [restore, before, after]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [restore, after](EditorSubsystem*, EditorProject*)
                        {
                            restore(after);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [restore, before](EditorSubsystem*, EditorProject*)
                        {
                            restore(before);
                        })
                };
            }));

    InitObject(action);

    // while the mode is up this is its own stack
    m_subsystem->GetSceneActionStack()->PushAction(action);
}

bool EditorArborState::IsGenerating() const
{
    return bool(m_work & (ArborWorkFlags::Growing | ArborWorkFlags::RegrowPending | ArborWorkFlags::BakingVariants));
}

bool EditorArborState::IsNewPlant() const
{
    return IsActive() && !m_sourcePrefab.IsValid() && !m_sourceSet.IsValid();
}

bool EditorArborState::CanBake() const
{
    return HasTarget() && m_subsystem->CanCreateAssets();
}

bool EditorArborState::Apply()
{
    AssertOnThread(g_simThread);

    if (!HasTarget())
    {
        return false;
    }

    if (IsGenerating())
    {
        m_statusText = "Busy";
        OnStateChanged();

        return false;
    }

    if (BakesVariants())
    {
        TResult<Array<Handle<Prefab>>> grown = GrowVariants(m_recipe, m_variantCount);

        if (grown.HasError())
        {
            m_statusText = HYP_FORMAT("Could not grow the variants: {}", grown.GetError().GetMessage());
            OnStateChanged();

            return false;
        }

        return BakeVariantSet(CollectVariantNodes(grown.GetValue().ToSpan()));
    }

    Handle<Node> root = CloneUntagged(m_plant.Get());

    if (!root.IsValid())
    {
        return false;
    }

    SetGrown(root.Get(), /* generated */ false);
    InitObject(root);

    if (m_sourcePrefab.IsValid())
    {
        root->SetName(m_sourcePrefab->GetName());

        m_sourcePrefab->SetRoot(root);

        GetCurrentAssetRegistry()->PutAssetsDeep(m_sourcePrefab);

        m_subsystem->SyncStalePrefabInstances();

        m_statusText = HYP_FORMAT("Baked {}", m_sourcePrefab->GetName());
    }
    else
    {
        Handle<Prefab> prefab = BakeNewPrefab(root);

        if (!prefab.IsValid())
        {
            return false;
        }

        // further bakes in this session update the prefab rather than making another
        m_sourcePrefab = prefab;

        m_statusText = HYP_FORMAT("Baked to new prefab {}", prefab->GetName());
    }

    HYP_LOG(Arbor, Info, "{}", m_statusText);

    m_lastBaked = m_sourcePrefab;
    m_appliedRecipe = m_recipe.ToRon();

    ResetActionStack();
    SetDirty(false);

    OnStateChanged();

    return true;
}

void EditorArborState::Bake()
{
    if (!CanBake())
    {
        return;
    }

    if (m_work & ArborWorkFlags::BakingVariants)
    {
        return;
    }

    if (IsGenerating())
    {
        m_work |= ArborWorkFlags::BakeRequested;
        m_statusText = "Baking once it has grown...";
        OnStateChanged();

        return;
    }

    if (BakesVariants())
    {
        StartBakingVariants();

        return;
    }

    if (Apply())
    {
        Exit(/* apply */ false);
    }
}

void EditorArborState::Revert()
{
    if (!HasTarget())
    {
        return;
    }

    RestoreRecipe(m_appliedRecipe);

    ResetActionStack();
    SetDirty(IsNewPlant());
}

Handle<Prefab> EditorArborState::BakeNewPrefab(const Handle<Node>& root)
{
    Handle<Prefab> prefab = MakeHandle<Prefab>(m_plant->GetName(), root);

    // registers the meshes, materials and textures it uses as well, under unique names
    GetCurrentAssetRegistry()->PutAssetsDeep(prefab);

    if (!prefab->IsRegistered())
    {
        HYP_LOG(Arbor, Error, "could not register prefab {}", m_plant->GetName());

        return Handle<Prefab>::Null();
    }

    FileAssetsDeep(prefab, MakeBakeFolder(prefab->GetName()));

    return prefab;
}

bool EditorArborState::BakesVariants() const
{
    return m_sourceSet.IsValid() || (IsNewPlant() && m_variantCount > 1);
}

Handle<Node> EditorArborState::CloneGrownPlant() const
{
    const Handle<Node> grownNode = FindGrownChild(m_plant.Get());

    if (!grownNode.IsValid())
    {
        return Handle<Node>::Null();
    }

    return CloneUntagged(grownNode.Get());
}

Handle<Node> EditorArborState::MakeVariantRoot(Name name, const ArborRecipeHandle& recipe, const Handle<Node>& grownNode) const
{
    Handle<Entity> root = MakeHandle<Entity>(name);
    WriteRecipe(root.Get(), m_kind, recipe.ToRon());

    grownNode->SetNodeFlags((grownNode->GetNodeFlags() & ~NodeFlags::Generated) | NodeFlags::HideInSceneOutline);
    root->AddChild(grownNode);

    InitObject(root);

    return root;
}

Array<Handle<Node>> EditorArborState::CollectVariantNodes(Span<const Handle<Prefab>> grown) const
{
    Array<Handle<Node>> grownNodes;
    grownNodes.PushBack(CloneGrownPlant());

    for (const Handle<Prefab>& prefab : grown)
    {
        grownNodes.PushBack(CloneUntagged(prefab->GetRoot().Get()));
    }

    return grownNodes;
}

bool EditorArborState::BakeVariantSet(const Array<Handle<Node>>& grownNodes)
{
    const Name plantName = m_plant->GetName();
    const uint64 baseSeed = m_recipe.GetSeed();
    const bool isNewSet = !m_sourceSet.IsValid();

    Handle<PrefabSet> prefabSet = isNewSet ? MakeHandle<PrefabSet>(NAME_FMT("{}_Variants", plantName)) : m_sourceSet;

    const Array<PrefabSetEntry> previousEntries = prefabSet->GetEntries();

    Array<PrefabSetEntry> entries;
    Array<Handle<Prefab>> added;

    for (uint32 index = 0; index < grownNodes.Size(); index++)
    {
        if (!grownNodes[index].IsValid())
        {
            return false;
        }

        Handle<Node> root = MakeVariantRoot(
            NAME_FMT("{}_{}", plantName, baseSeed + index),
            MakeVariantRecipe(m_recipe, index),
            grownNodes[index]);

        PrefabSetEntry entry;

        if (index < previousEntries.Size() && previousEntries[index].prefab.IsValid())
        {
            entry = previousEntries[index];

            root->SetName(entry.prefab->GetName());
            entry.prefab->SetRoot(root);
        }
        else
        {
            entry.prefab = MakeHandle<Prefab>(root->GetName(), root);
            added.PushBack(entry.prefab);
        }

        entries.PushBack(entry);
    }

    prefabSet->SetEntries(entries);

    GetCurrentAssetRegistry()->PutAssetsDeep(prefabSet);

    if (!prefabSet->IsRegistered())
    {
        HYP_LOG(Arbor, Error, "could not register {}", prefabSet->GetName());

        return false;
    }

    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (isNewSet)
    {
        FileAssetsDeep(prefabSet, MakeBakeFolder(prefabSet->GetName()));
    }
    else if (project.IsValid() && project->GetAssetFolders().IsValid())
    {
        const String folder = project->GetAssetFolders()->GetAssetFolder(prefabSet->GetPath());

        for (const Handle<Prefab>& prefab : added)
        {
            FileAssetsDeep(prefab, folder);
        }
    }

    m_subsystem->SyncStalePrefabInstances();

    m_sourceSet = prefabSet;
    m_lastBaked = prefabSet;

    m_statusText = HYP_FORMAT("Baked {} variants into {}", entries.Size(), prefabSet->GetName());
    HYP_LOG(Arbor, Info, "{}", m_statusText);

    m_appliedRecipe = m_recipe.ToRon();

    ResetActionStack();
    SetDirty(false);

    OnStateChanged();

    return true;
}

void EditorArborState::StartBakingVariants()
{
    m_work |= ArborWorkFlags::BakingVariants;
    m_statusText = HYP_FORMAT("Generating... ({} variants)", m_variantCount);
    OnStateChanged();

    const WeakHandle<EditorArborState> weakThis = WeakHandleFromThis();
    const String bakedRecipe = m_recipe.ToRon();
    const uint32 variantCount = m_variantCount;

    UniquePtr<ArborRecipeHandle> job = MakeUnique<ArborRecipeHandle>(m_recipe.Clone());

    TaskSystem::GetInstance().Enqueue(
        [weakThis, bakedRecipe, variantCount, job = std::move(job)]() mutable
        {
            Array<Handle<Prefab>> grown;
            String error;

            TResult<Array<Handle<Prefab>>> result = GrowVariants(*job, variantCount);

            if (result.HasError())
            {
                error = result.GetError().GetMessage();
            }
            else
            {
                grown = std::move(result.GetValue());
            }

            job.Reset();

            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [weakThis, bakedRecipe, error, grown = std::move(grown)]() mutable
                {
                    if (Handle<EditorArborState> state = weakThis.Lock(); state.IsValid())
                    {
                        state->FinishBakingVariants(std::move(grown), error, bakedRecipe);
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void EditorArborState::FinishBakingVariants(Array<Handle<Prefab>>&& grown, const String& error, const String& bakedRecipe)
{
    if (!(m_work & ArborWorkFlags::BakingVariants))
    {
        return;
    }

    m_work &= ~ArborWorkFlags::BakingVariants;

    if (!IsActive() || error.Any() || grown.Size() + 1 != m_variantCount)
    {
        m_statusText = error.Any() ? HYP_FORMAT("Could not grow the variants: {}", error) : String("Baking was interrupted");
        OnStateChanged();

        return;
    }

    if (m_recipe.ToRon() != bakedRecipe)
    {
        m_statusText = "The plant changed while baking: bake again";
        OnStateChanged();

        return;
    }

    if (BakeVariantSet(CollectVariantNodes(grown.ToSpan())))
    {
        Exit(/* apply */ false);
    }
}

bool EditorArborState::GrowNow()
{
    if (!HasTarget() || (m_work & ArborWorkFlags::Growing))
    {
        return false;
    }

    m_work &= ~ArborWorkFlags::RegrowPending;
    m_work |= ArborWorkFlags::Growing;

    GrownPlant grown;
    grown.Take(GrowPrefab(m_recipe));

    FinishGrowing(m_plant, std::move(grown));

    return CloneGrownPlant().IsValid();
}

TResult<Handle<Prefab>> EditorArborState::GrowPrefab(const ArborRecipeHandle& recipe)
{
    ByteBuffer glb;

    if (!recipe.GenerateGlb(glb))
    {
        return HYP_MAKE_ERROR(Error, "Arbor could not grow it: {}", ArborRecipeHandle::GetLastError());
    }

    const FilePath virtualPath = FilePath(HYP_FORMAT("{}_{}.glb", recipe.GetName(), recipe.GetSeed()));

    AssetLoadResult result = GLTFModelLoader::LoadFromMemory(glb.ToByteView(), virtualPath, AssetLoadHint::Transient);

    if (result.HasError())
    {
        return HYP_MAKE_ERROR(Error, "the grown plant did not load: {}", result.GetError().GetMessage());
    }

    LoadedAsset& loaded = result.GetValue();
    loaded.OnPostLoad();

    Handle<Prefab> prefab = loaded.ExtractAs<Prefab>();

    if (!prefab.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "the grown plant loaded as something other than a prefab");
    }

    return prefab;
}

String EditorArborState::MakeBakeFolder(Name name) const
{
    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (!project.IsValid() || !project->GetAssetFolders().IsValid())
    {
        return String::empty;
    }

    const String& parentFolder = m_subsystem->GetCurrentAssetFolder();

    return EditorAssetFolders::JoinFolderPath(parentFolder, project->GetAssetFolders()->MakeUniqueFolderName(parentFolder, String(*name)));
}

void EditorArborState::FileAssetsDeep(const Handle<AssetObject>& asset, const String& folder)
{
    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (!project.IsValid() || !project->GetAssetFolders().IsValid())
    {
        return;
    }

    EditorAssetFolders& assetFolders = *project->GetAssetFolders();

    Set<uint32, EditorAllocator> changedBuckets;

    auto fileAsset = [&](const Handle<AssetObject>& found)
    {
        if (!found.IsValid() || !found->IsRegistered() || found->GetPath().registryId != AssetRegistryId::Game)
        {
            return;
        }

        assetFolders.SetAssetFolder(found->GetPath(), folder);
        changedBuckets.Insert(found->GetPath().GetBucket().GetIndex());
    };

    fileAsset(asset);
    AssetRegistry::WalkAssetDeep(BoxedValue(asset), fileAsset);

    for (uint32 bucketIndex : changedBuckets)
    {
        m_subsystem->OnAssetsChanged(bucketIndex);
    }
}

String EditorArborState::GetStatusText() const
{
    return m_statusText;
}

void EditorArborState::RebuildControls()
{
    m_controls.Clear();

    const uint32 count = m_recipe.GetControlCount();

    for (uint32 index = 0; index < count; index++)
    {
        ArborControlDesc desc;

        if (m_recipe.GetControl(index, desc))
        {
            m_controls.PushBack(std::move(desc));
        }
    }

    m_layoutVersion++;
}

void EditorArborState::RecipeChanged(bool reshaped)
{
    if (m_plant.IsValid())
    {
        WriteRecipe(m_plant.Get(), m_kind, m_recipe.ToRon());
    }

    if (reshaped)
    {
        RebuildControls();
    }

    m_work |= ArborWorkFlags::RegrowPending;
    m_regrowTimer.Reset();

    OnStateChanged();
}

void EditorArborState::RestoreRecipe(const String& ron)
{
    m_recipe = ArborRecipeHandle::FromRon(uint32(m_kind), ron);
    m_committedRecipe = ron;

    RecipeChanged(/* reshaped */ true);
}

void EditorArborState::Update()
{
    if (!IsActive())
    {
        return;
    }

    // undoing back to where a new plant started empties the undo stack, which clears dirty; it is still unsaved
    if (IsNewPlant() && !IsDirty())
    {
        SetDirty(true);
    }

    if ((m_work & ArborWorkFlags::Growing)
        || !(m_work & ArborWorkFlags::RegrowPending)
        || m_regrowTimer.Interval(ClockTimer::Now()) < RegrowDelay)
    {
        return;
    }

    m_work &= ~ArborWorkFlags::RegrowPending;

    StartGrowing(m_recipe.Clone());
}

void EditorArborState::StartGrowing(ArborRecipeHandle&& recipe)
{
    m_work |= ArborWorkFlags::Growing;
    m_statusText = "Generating...";
    OnStateChanged();

    const WeakHandle<EditorArborState> weakThis = WeakHandleFromThis();
    const WeakHandle<Entity> weakPlant = MakeWeakRef(m_plant);

    UniquePtr<ArborRecipeHandle> job = MakeUnique<ArborRecipeHandle>(std::move(recipe));

    TaskSystem::GetInstance().Enqueue(
        [weakThis, weakPlant, job = std::move(job)]() mutable
        {
            const ClockTimer timer;

            GrownPlant grown;
            grown.Take(GrowPrefab(*job));

            grown.milliseconds = timer.IntervalHighPrec(ClockTimer::Now()) * 1000.0;

            job.Reset();

            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [weakThis, weakPlant, grown = std::move(grown)]() mutable
                {
                    if (Handle<EditorArborState> state = weakThis.Lock(); state.IsValid())
                    {
                        state->FinishGrowing(weakPlant.Lock(), std::move(grown));
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void EditorArborState::FinishGrowing(const Handle<Entity>& plant, GrownPlant&& grown)
{
    m_work &= ~ArborWorkFlags::Growing;

    if (!plant.IsValid() || plant != m_plant)
    {
        return;
    }

    if (grown.error.Any() || !grown.prefab.IsValid())
    {
        HYP_LOG(Arbor, Warning, "could not grow {}: {}", plant->GetName(), grown.error);

        m_work &= ~ArborWorkFlags::BakeRequested;
        m_statusText = HYP_FORMAT("Could not grow: {}", grown.error);
        OnStateChanged();

        return;
    }

    const Node::NodeList previous = plant->GetChildren();

    for (const Handle<Node>& child : previous)
    {
        if (IsGrownNode(child))
        {
            child->Remove();
        }
    }

    Handle<Node> grownNode = CloneUntagged(grown.prefab->GetRoot().Get());

    if (grownNode.IsValid())
    {
        grownNode->SetNodeFlags(grownNode->GetNodeFlags() | NodeFlags::Generated | NodeFlags::HideInSceneOutline);

        plant->AddChild(grownNode);
    }

    m_statusText = HYP_FORMAT("Grown in {}ms", uint32(grown.milliseconds));

    if ((m_work & ArborWorkFlags::BakeRequested) && !(m_work & ArborWorkFlags::RegrowPending))
    {
        m_work &= ~ArborWorkFlags::BakeRequested;

        Bake();

        return;
    }

    OnStateChanged();
}

} // namespace Hyperion
