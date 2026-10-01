/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Prefab/EditorPrefabEditState.hpp>
#include <Editor/Csg/EditorCsgState.hpp>
#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Node.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Light/Light.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

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

void EditorPrefabEditState::Initialize(EditorSubsystem* subsystem)
{
    m_subsystem = subsystem;
}

bool EditorPrefabEditState::IsActive() const
{
    return m_prefab.IsValid() && m_editScene.IsValid();
}

bool EditorPrefabEditState::CanEnter() const
{
    return m_subsystem != nullptr
        && m_subsystem->GetCurrentProject().IsValid()
        && m_subsystem->GetProjectWorld().IsValid()
        && !m_subsystem->IsSimulating();
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

    if (!CanEnter())
    {
        return false;
    }

    m_subsystem->ExitMeshEditMode(/* saveEdits */ true);
    m_subsystem->GetCsgState()->Exit(/* saveEdits */ true);
    m_subsystem->GetTerrainState()->SetEnabled(false);
    m_subsystem->DisableSurfacePainters();

    m_previousActiveScene = m_subsystem->GetActiveScene();

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    m_prefab = prefab;

    CreateScenes();
    SetWorkingRoot(CloneWorkingRoot());

    ResetActionStack();
    SetDirty(false);

    m_subsystem->UpdateViewportIsolation();

    FrameCamera();

    m_subsystem->SetActiveScene(m_editScene);

    HYP_LOG(Editor, Info, "Editing prefab '{}'", m_prefab->GetName());

    OnActiveChanged(true);

    return true;
}

void EditorPrefabEditState::Exit(bool apply)
{
    AssertOnThread(g_simThread);

    if (!IsActive())
    {
        return;
    }

    if (apply && m_dirty)
    {
        Apply();
    }

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    Handle<Scene> previousActiveScene = m_previousActiveScene.Lock();

    if (!previousActiveScene.IsValid())
    {
        for (const Handle<Scene>& scene : m_subsystem->GetProjectWorld()->GetScenes())
        {
            if (scene.IsValid() && (scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                previousActiveScene = scene;

                break;
            }
        }
    }

    m_prefab.Reset();
    m_previousActiveScene.Reset();

    m_actionStackStateHandler.Reset();
    m_actionStack.Reset();

    m_subsystem->UpdateViewportIsolation();

    RestoreCamera();

    m_subsystem->SetActiveScene(previousActiveScene);
    m_subsystem->OnActiveActionStackChanged();

    // removed only once the active scene has moved off it
    DestroyScenes();

    SetDirty(false);

    OnActiveChanged(false);
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

bool EditorPrefabEditState::IsDirty() const
{
    return m_dirty;
}

const Handle<Prefab>& EditorPrefabEditState::GetPrefab() const
{
    return m_prefab;
}

String EditorPrefabEditState::GetPrefabName() const
{
    return m_prefab.IsValid() ? String(*m_prefab->GetName()) : String::empty;
}

const Handle<Scene>& EditorPrefabEditState::GetEditScene() const
{
    return m_editScene;
}

bool EditorPrefabEditState::IsEditScene(const Handle<Scene>& scene) const
{
    return scene.IsValid() && scene == m_editScene;
}

EditorActionStack* EditorPrefabEditState::GetActionStack() const
{
    return IsActive() ? m_actionStack.Get() : nullptr;
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

Array<Handle<Scene>> EditorPrefabEditState::GetIsolatedScenes() const
{
    if (!IsActive())
    {
        return {};
    }

    return { m_editScene, m_stageScene };
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

void EditorPrefabEditState::CreateScenes()
{
    const Handle<World>& world = m_subsystem->GetProjectWorld();
    Assert(world.IsValid());

    m_editScene = MakeHandle<Scene>(NAME_FMT("PrefabEdit_{}", m_prefab->GetName()), SceneFlags::EDITOR | SceneFlags::HAS_OCTREE);
    m_editScene->SetIsTransient(true);
    InitObject(m_editScene);

    world->AddScene(m_editScene, /* addToStreamingLayer */ false);

    m_stageScene = MakeHandle<Scene>(NAME("PrefabEditStage"), SceneFlags::EDITOR);
    m_stageScene->SetIsTransient(true);
    InitObject(m_stageScene);

    Handle<DirectionalLight> keyLight = MakeHandle<DirectionalLight>(
        Vec3f(-0.4f, 0.75f, 0.5f).Normalized(),
        Color(1.0f, 0.98f, 0.94f, 1.0f),
        4.0f);

    keyLight->SetName(NAME("PrefabEditKeyLight"));
    keyLight->SetLightFlags(LightFlags::None);
    InitObject(keyLight);
    m_stageScene->GetRoot()->AddChild(keyLight);

    Handle<DirectionalLight> fillLight = MakeHandle<DirectionalLight>(
        Vec3f(0.5f, 0.2f, -0.8f).Normalized(),
        Color(0.6f, 0.7f, 0.85f, 1.0f),
        1.5f);

    fillLight->SetName(NAME("PrefabEditFillLight"));
    fillLight->SetLightFlags(LightFlags::None);
    InitObject(fillLight);
    m_stageScene->GetRoot()->AddChild(fillLight);

    world->AddScene(m_stageScene, /* addToStreamingLayer */ false);
}

void EditorPrefabEditState::DestroyScenes()
{
    const Handle<World>& world = m_subsystem->GetProjectWorld();

    for (Handle<Scene>* scene : { &m_editScene, &m_stageScene })
    {
        if (scene->IsValid() && world.IsValid())
        {
            world->RemoveScene(scene->Get());
        }

        scene->Reset();
    }
}

void EditorPrefabEditState::FrameCamera()
{
    EditorViewport* viewport = m_subsystem->GetActiveViewport();

    if (!viewport || !viewport->GetCamera().IsValid())
    {
        return;
    }

    const Handle<Camera>& camera = viewport->GetCamera();

    m_savedCameraTranslation = camera->GetWorldTranslation();
    m_savedCameraDirection = camera->GetDirection();
    m_hasSavedCamera = true;

    const BoundingBox bounds = m_editScene->GetRoot()->GetWorldBounds();

    const Vec3f center = bounds.IsValid() ? bounds.GetCenter() : Vec3f::Zero();
    const float radius = bounds.IsValid() ? MathUtil::Max(bounds.GetRadius(), 0.5f) : 1.0f;

    const Vec3f viewDirection = Vec3f(-1.0f, -0.6f, -1.0f).Normalized();

    camera->SetWorldTranslation(center - viewDirection * (radius * 2.5f));
    camera->SetDirection(viewDirection);
}

void EditorPrefabEditState::RestoreCamera()
{
    if (!m_hasSavedCamera)
    {
        return;
    }

    m_hasSavedCamera = false;

    EditorViewport* viewport = m_subsystem->GetActiveViewport();

    if (!viewport || !viewport->GetCamera().IsValid())
    {
        return;
    }

    viewport->GetCamera()->SetWorldTranslation(m_savedCameraTranslation);
    viewport->GetCamera()->SetDirection(m_savedCameraDirection);
}

void EditorPrefabEditState::ResetActionStack()
{
    m_actionStackStateHandler.Reset();

    m_actionStack = MakeHandle<EditorActionStack>(m_subsystem->GetCurrentProject().ToWeak());

    m_actionStackStateHandler = m_actionStack->OnStateChange.Bind(
        this,
        [this](EnumFlags<EditorActionStackState>, int)
        {
            SetDirty(m_actionStack.IsValid() && m_actionStack->CanUndo());
        });

    m_subsystem->OnActiveActionStackChanged();
}

void EditorPrefabEditState::SetDirty(bool dirty)
{
    if (m_dirty == dirty)
    {
        return;
    }

    m_dirty = dirty;

    OnDirtyChanged(dirty);
}

} // namespace Hyperion
