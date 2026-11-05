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

#include <EditorArborState.generated.inl>

namespace Hyperion {

ARBOREDITOR_API HYP_DECLARE_LOG_CHANNEL(Arbor);

namespace {

constexpr TimeDiff RegrowDelay { 80 };

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
    const Prefab* prefab = asset.IsValid() ? TryCast<Prefab>(asset.Get()) : nullptr;

    return prefab && prefab->GetRoot().IsValid() && prefab->GetRoot()->HasDataEntry(g_nameArborRecipe);
}

bool EditorArborState::EnterAsset(const Handle<AssetObject>& asset)
{
    return CanEditAsset(asset) && EnterPrefab(TryCast<Prefab>(asset));
}

Handle<AssetObject> EditorArborState::GetInspectedAsset() const
{
    return m_sourcePrefab;
}

String EditorArborState::GetDisplayName() const
{
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

    Handle<Entity> plant = TryCast<Entity>(prefab->GetRoot()->Clone());

    if (!plant.IsValid())
    {
        return false;
    }

    Prefab::UntagAsPrefabInstance(plant.Get());
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
    m_regrowPending = false;

    OnStateChanged();

    return true;
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
    m_bakeRequested = false;
    m_regrowPending = true;
    m_regrowRequestedAt = Time::Now() - RegrowDelay;

    RebuildControls();

    FinishEnter();

    OnStateChanged();

    return true;
}

void EditorArborState::ClearTarget()
{
    m_plant.Reset();
    m_sourcePrefab.Reset();
    m_recipe = ArborRecipeHandle();
    m_controls.Clear();
    m_layoutVersion++;
    m_regrowPending = false;
    m_bakeRequested = false;
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
    const uint64 ticks = uint64(Clock::now().time_since_epoch().count());

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

void EditorArborState::SetControlRanged(uint32 index, bool isRange, float lo, float hi)
{
    ArborControlValue value;
    value.isRange = isRange;
    value.lo = lo;
    value.hi = isRange ? hi : lo;

    bool reshaped = false;

    if (!HasTarget() || !m_recipe.SetValue(index, value, reshaped))
    {
        return;
    }

    RecipeChanged(reshaped);
}

void EditorArborState::SetControlInteger(uint32 index, uint32 integer)
{
    ArborControlValue value;
    value.integer = integer;

    bool reshaped = false;

    if (!HasTarget() || !m_recipe.SetValue(index, value, reshaped))
    {
        return;
    }

    RecipeChanged(reshaped);
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

        if (!state.IsValid() || !state->IsActive() || state->m_plant != weakPlant.Lock() || state->m_recipe.ToRon() == ron)
        {
            return;
        }

        state->m_recipe = ArborRecipeHandle::FromRon(uint32(state->m_kind), ron);
        state->m_committedRecipe = ron;
        state->RecipeChanged(/* reshaped */ true);
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
    return m_generating || m_regrowPending;
}

bool EditorArborState::IsNewPlant() const
{
    return IsActive() && !m_sourcePrefab.IsValid();
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

    Handle<Node> root = m_plant->Clone();

    if (!root.IsValid())
    {
        return false;
    }

    Prefab::UntagAsPrefabInstance(root.Get());
    SetGrown(root.Get(), /* generated */ false);
    InitObject(root);

    if (m_sourcePrefab.IsValid())
    {
        root->SetName(m_sourcePrefab->GetName());

        m_sourcePrefab->SetRoot(root);
        m_sourcePrefab->SetRevision(m_sourcePrefab->GetRevision() + 1);

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

    if (IsGenerating())
    {
        m_bakeRequested = true;
        m_statusText = "Baking once it has grown...";
        OnStateChanged();

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

    m_recipe = ArborRecipeHandle::FromRon(uint32(m_kind), m_appliedRecipe);
    m_committedRecipe = m_appliedRecipe;

    RecipeChanged(/* reshaped */ true);

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

    FileBakedAssets(prefab);

    return prefab;
}

// filed as an import is: the prefab and all it uses in a folder named after it, in the content browser's current folder
void EditorArborState::FileBakedAssets(const Handle<Prefab>& prefab)
{
    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (!project.IsValid() || !project->GetAssetFolders().IsValid())
    {
        return;
    }

    EditorAssetFolders& assetFolders = *project->GetAssetFolders();

    const String& parentFolder = m_subsystem->GetCurrentAssetFolder();
    const String folder = EditorAssetFolders::JoinFolderPath(parentFolder, assetFolders.MakeUniqueFolderName(parentFolder, String(*prefab->GetName())));

    Set<uint32, EditorAllocator> changedBuckets;

    auto fileAsset = [&](const Handle<AssetObject>& asset)
    {
        if (!asset.IsValid() || !asset->IsRegistered() || asset->GetPath().registryId != AssetRegistryId::Game)
        {
            return;
        }

        assetFolders.SetAssetFolder(asset->GetPath(), folder);
        changedBuckets.Insert(asset->GetPath().GetBucket().GetIndex());
    };

    fileAsset(prefab);
    AssetRegistry::WalkAssetDeep(BoxedValue(prefab), fileAsset);

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

    m_regrowPending = true;
    m_regrowRequestedAt = Clock::now();

    OnStateChanged();
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

    if (m_generating
        || !m_regrowPending
        || Time::Now() - m_regrowRequestedAt < RegrowDelay)
    {
        return;
    }

    m_regrowPending = false;

    StartGrowing(m_recipe.Clone());
}

void EditorArborState::StartGrowing(ArborRecipeHandle&& recipe)
{
    m_generating = true;
    m_statusText = "Growing...";
    OnStateChanged();

    const WeakHandle<EditorArborState> weakThis = WeakHandleFromThis();
    const WeakHandle<Entity> weakPlant = MakeWeakRef(m_plant);

    UniquePtr<ArborRecipeHandle> job = MakeUnique<ArborRecipeHandle>(std::move(recipe));

    TaskSystem::GetInstance().Enqueue(
        [weakThis, weakPlant, job = std::move(job)]() mutable
        {
            const Clock::time_point startedAt = Clock::now();

            GrownPlant grown;
            ByteBuffer glb;

            if (!job->GenerateGlb(glb))
            {
                grown.error = ArborRecipeHandle::GetLastError();
            }
            else
            {
                const FilePath virtualPath = FilePath(HYP_FORMAT("{}_{}.glb", job->GetName(), job->GetSeed()));

                AssetLoadResult result = GLTFModelLoader::LoadFromMemory(glb.ToByteView(), virtualPath, AssetLoadHint::Transient);

                if (result.HasError())
                {
                    grown.error = result.GetError().GetMessage();
                }
                else
                {
                    LoadedAsset& loaded = result.GetValue();
                    loaded.OnPostLoad();

                    grown.prefab = loaded.ExtractAs<Prefab>();
                }
            }

            grown.milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - startedAt).count();

            job.Reset();

            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [weakThis, weakPlant, grown = std::move(grown)]() mutable
                {
                    if (Handle<EditorArborState> state = weakThis.Lock())
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
    m_generating = false;

    // the mode was left, or another plant entered, while this one grew
    if (!plant.IsValid() || plant != m_plant)
    {
        return;
    }

    if (grown.error.Any() || !grown.prefab.IsValid())
    {
        HYP_LOG(Arbor, Warning, "could not grow {}: {}", plant->GetName(), grown.error);

        m_bakeRequested = false;
        m_statusText = HYP_FORMAT("Could not grow: {}", grown.error);
        OnStateChanged();

        return;
    }

    const Node::NodeList previous = plant->GetChildren();

    for (const Handle<Node>& child : previous)
    {
        if (child.IsValid() && (child->GetNodeFlags() & NodeFlags::Generated))
        {
            child->Remove();
        }
    }

    Handle<Node> grownNode = grown.prefab->GetRoot()->Clone();

    if (grownNode.IsValid())
    {
        Prefab::UntagAsPrefabInstance(grownNode.Get());
        grownNode->SetNodeFlags(grownNode->GetNodeFlags() | NodeFlags::Generated | NodeFlags::HideInSceneOutline);

        plant->AddChild(grownNode);
    }

    m_statusText = HYP_FORMAT("Grown in {}ms", uint32(grown.milliseconds));

    if (m_bakeRequested && !m_regrowPending)
    {
        m_bakeRequested = false;

        Bake();

        return;
    }

    OnStateChanged();
}

} // namespace Hyperion
