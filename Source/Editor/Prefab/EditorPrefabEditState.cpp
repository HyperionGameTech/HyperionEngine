/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Prefab/EditorPrefabEditState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Node.hpp>
#include <Scene/Prefab.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorPrefabEditState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

struct PrefabInstanceReplacement
{
    Handle<Node> instance;
    Handle<Node> replacement;
    WeakHandle<Node> parent;
    Handle<Prefab> ownerPrefab;
};

void DeselectNode(EditorSubsystem& subsystem, const Handle<Node>& node)
{
    if (subsystem.GetFocusedNode() == node)
    {
        subsystem.SetFocusedNode(Handle<Node>::Null());
    }

    subsystem.RemoveFromSelection(node);
}

void SwapNodes(EditorSubsystem& subsystem, const Handle<Node>& from, const Handle<Node>& to, const WeakHandle<Node>& parentWeak)
{
    Handle<Node> parent = parentWeak.Lock();

    if (!parent.IsValid())
    {
        return;
    }

    DeselectNode(subsystem, from);

    from->Remove();
    parent->AddChild(to);
}

void SwapReplacements(
    EditorSubsystem& subsystem,
    const Array<PrefabInstanceReplacement>& replacements,
    bool revert)
{
    Array<Prefab*> changedOwners;

    for (const PrefabInstanceReplacement& record : replacements)
    {
        if (revert)
        {
            SwapNodes(subsystem, record.replacement, record.instance, record.parent);
        }
        else
        {
            SwapNodes(subsystem, record.instance, record.replacement, record.parent);
        }

        if (record.ownerPrefab.IsValid() && !changedOwners.Contains(record.ownerPrefab.Get()))
        {
            changedOwners.PushBack(record.ownerPrefab.Get());
        }
    }

    for (Prefab* ownerPrefab : changedOwners)
    {
        ownerPrefab->MarkDirty();

        Prefab::OnPrefabChanged(ownerPrefab);
    }
}

Handle<Node> MakeReplacement(
    const Node& newRoot,
    const Prefab& prefab,
    uint32 revision,
    const Node& instance)
{
    Handle<Node> replacement = newRoot.Clone();

    if (!replacement.IsValid())
    {
        return replacement;
    }

    Prefab::TagAsPrefabInstance(replacement.Get(), prefab.GetUUID(), revision);

    replacement->SetName(instance.GetName());
    replacement->SetLocalTransform(instance.GetLocalTransform());
    replacement->SetUUID(instance.GetUUID());

    return replacement;
}

} // namespace

EditorPrefabEditState::EditorPrefabEditState() = default;

EditorPrefabEditState::~EditorPrefabEditState() = default;

bool EditorPrefabEditState::IsActive() const
{
    return m_prefab.IsValid() && m_editScene.IsValid();
}

bool EditorPrefabEditState::Enter(const Handle<Prefab>& prefab)
{
    AssertOnThread(g_simThread);

    if (!prefab.IsValid() || !prefab->GetRoot().IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot edit prefab: it has no root node");

        return false;
    }

    if (IsActive())
    {
        // Switching to another prefab is the caller's job, since it has to decide what happens to pending edits
        return m_prefab == prefab;
    }

    if (!BeginEnter())
    {
        return false;
    }

    m_prefab = prefab;

    CreateScenes(NAME_FMT("PrefabEdit_{}", m_prefab->GetName()));
    SetWorkingRoot(CloneWorkingRoot());

    HYP_LOG(Editor, Info, "Editing prefab '{}'", m_prefab->GetName());

    FinishEnter();

    return true;
}

void EditorPrefabEditState::ClearTarget()
{
    m_prefab.Reset();
}

void EditorPrefabEditState::Apply()
{
    AssertOnThread(g_simThread);

    if (!IsActive())
    {
        return;
    }

    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (!project.IsValid())
    {
        return;
    }

    Handle<Node> newRoot = m_editScene->GetRoot()->Clone();

    if (!newRoot.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to clone the working tree of prefab '{}'", m_prefab->GetName());

        return;
    }

    Prefab::UntagAsPrefabInstance(newRoot.Get());

    Handle<Prefab> prefab = m_prefab;
    Handle<Node> previousRoot = prefab->GetRoot();

    const uint32 previousRevision = prefab->GetRevision();
    const uint32 newRevision = previousRevision + 1;

    Array<PrefabInstanceReplacement> replacements;

    if (m_updateInstancesOnApply)
    {
        for (const Handle<Node>& instance : prefab->FindLiveInstances(m_subsystem->GetProjectWorld().Get()))
        {
            Node* parent = instance->GetParent();

            Handle<Node> replacement = parent
                ? MakeReplacement(*newRoot, *prefab, newRevision, *instance)
                : Handle<Node>::Null();

            if (replacement.IsValid())
            {
                replacements.PushBack(PrefabInstanceReplacement { instance, replacement, MakeWeakRef(parent) });
            }
        }

        Array<AssetDesc> prefabDescs;
        GetCurrentAssetRegistry()->GetBucketAssetDescs(AssetBuckets::Prefabs.GetIndex(), prefabDescs);

        for (const AssetDesc& prefabDesc : prefabDescs)
        {
            Handle<Prefab> ownerPrefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, prefabDesc.name);

            if (!ownerPrefab.IsValid() || ownerPrefab == prefab || !ownerPrefab->GetRoot().IsValid())
            {
                continue;
            }

            for (Node* nested : ownerPrefab->GetRoot()->GetDescendants())
            {
                Node* parent = nested->GetParent();

                if (!parent || Prefab::GetSourcePrefabUUID(nested) != prefab->GetUUID())
                {
                    continue;
                }

                Handle<Node> replacement = MakeReplacement(
                    *newRoot,
                    *prefab,
                    newRevision,
                    *nested);

                if (replacement.IsValid())
                {
                    replacements.PushBack(PrefabInstanceReplacement { MakeStrongRef(nested), replacement, MakeWeakRef(parent), ownerPrefab });
                }
            }
        }
    }

    const uint32 numInstances = uint32(replacements.Size());

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("Edit Prefab {}", prefab->GetName()),
        Proc<EditorActionFunctions()>(
            [prefab, newRoot, previousRoot, previousRevision, newRevision, replacements = std::move(replacements)]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [prefab, newRoot, newRevision, replacements](EditorSubsystem* subsystem, EditorProject*)
                        {
                            prefab->SetRoot(newRoot);
                            prefab->SetRevision(newRevision);

                            GetCurrentAssetRegistry()->PutAssetsDeep(prefab);

                            SwapReplacements(*subsystem, replacements, /* revert */ false);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [prefab, previousRoot, previousRevision, replacements](EditorSubsystem* subsystem, EditorProject*)
                        {
                            prefab->SetRoot(previousRoot);
                            prefab->SetRevision(previousRevision);

                            SwapReplacements(*subsystem, replacements, /* revert */ true);

                            subsystem->SyncStalePrefabInstances();
                        })
                };
            }));

    InitObject(action);
    project->GetActionStack()->PushAction(action);

    HYP_LOG(Editor, Info, "Applied edits to prefab '{}' ({} instances updated)", prefab->GetName(), numInstances);

    ResetActionStack();
    SetDirty(false);
}

void EditorPrefabEditState::Revert()
{
    AssertOnThread(g_simThread);

    if (!IsActive())
    {
        return;
    }

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    SetWorkingRoot(CloneWorkingRoot());

    ResetActionStack();
    SetDirty(false);
}

const Handle<Prefab>& EditorPrefabEditState::GetPrefab() const
{
    return m_prefab;
}

String EditorPrefabEditState::GetPrefabName() const
{
    return m_prefab.IsValid() ? String(*m_prefab->GetName()) : String::empty;
}

String EditorPrefabEditState::GetDisplayName() const
{
    return GetPrefabName();
}

String EditorPrefabEditState::GetKindName() const
{
    return "prefab";
}

uint32 EditorPrefabEditState::GetNumLiveInstances() const
{
    if (!m_prefab.IsValid())
    {
        return 0;
    }

    return uint32(m_prefab->FindLiveInstances(m_subsystem->GetProjectWorld().Get()).Size());
}

bool EditorPrefabEditState::IsUpdateInstancesOnApply() const
{
    return m_updateInstancesOnApply;
}

void EditorPrefabEditState::SetUpdateInstancesOnApply(bool updateInstancesOnApply)
{
    m_updateInstancesOnApply = updateInstancesOnApply;
}

Handle<Node> EditorPrefabEditState::CloneWorkingRoot() const
{
    Handle<Node> workingRoot = m_prefab->GetRoot()->Clone();

    if (!workingRoot.IsValid())
    {
        return Handle<Node>::Null();
    }

    // the template root can carry a source tag when it was synced from an instance
    Prefab::UntagAsPrefabInstance(workingRoot.Get());
    workingRoot->SetName(m_prefab->GetName());

    InitObject(workingRoot);

    return workingRoot;
}

void EditorPrefabEditState::SetWorkingRoot(const Handle<Node>& workingRoot)
{
    if (!m_editScene.IsValid() || !workingRoot.IsValid())
    {
        return;
    }

    m_editScene->SetRoot(workingRoot);
}

} // namespace Hyperion
