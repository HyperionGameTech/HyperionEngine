/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Node.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/DetachedScene.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Containers/Map.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Prefab.generated.inl>

namespace Hyperion {

static const Name s_namePrefabSource = NAME("PrefabSource");
static const Name s_namePrefabRevision = NAME("PrefabRevision");

Delegate<void, Prefab*> Prefab::OnPrefabChanged;

void Prefab_OnPostLoad(Prefab& prefab)
{
    if (!EngineGlobals::IsShuttingDown())
    {
        const Handle<Node>& root = prefab.GetRoot();

        if (root.IsValid())
        {
            prefab.SyncRootName();

            root->SetScene(&GetDetachedSceneForThread(g_simThread));
        }
    }
}

namespace {

void CollectStaleInstances(Node& node, Map<UUID, Handle<Prefab>>& prefabCache, Array<Pair<Handle<Node>, Handle<Prefab>>>& outStale)
{
    for (Node* child : node.GetChildren())
    {
        if (!child)
        {
            continue;
        }

        const UUID prefabUUID = Prefab::GetSourcePrefabUUID(child);

        if (prefabUUID != UUID::Invalid())
        {
            auto it = prefabCache.Find(prefabUUID);

            if (it == prefabCache.End())
            {
                it = prefabCache.Insert(prefabUUID, Prefab::FindByUUID(prefabUUID)).first;
            }

            const Handle<Prefab>& prefab = it->second;

            if (prefab.IsValid() && prefab->GetRoot().IsValid() && Prefab::GetInstanceRevision(child) != prefab->GetRevision())
            {
                outStale.PushBack({ MakeStrongRef(child), prefab });

                continue;
            }
        }

        CollectStaleInstances(*child, prefabCache, outStale);
    }
}

} // namespace

#pragma region Prefab

Prefab::Prefab()
    : Prefab(Name::Invalid())
{
}

Prefab::Prefab(Name name, const Handle<Node>& root)
    : AssetObject(name),
      m_root(root)
{
}

Result Prefab::Rename(Name name)
{
    Result result = AssetObject::Rename(name);

    SyncRootName();

    return result;
}

const Handle<Node>& Prefab::GetRoot() const
{
    return m_root;
}

void Prefab::SetRoot(const Handle<Node>& root)
{
    if (root == m_root)
    {
        return;
    }

    m_root = root;
    m_revision++;

    SyncRootName();

    MarkDirty();

    OnPrefabChanged(this);
}

uint32 Prefab::GetRevision() const
{
    return m_revision;
}

void Prefab::SetRevision(uint32 revision)
{
    if (revision == m_revision)
    {
        return;
    }

    m_revision = revision;

    MarkDirty();
}

void Prefab::SyncRootName()
{
    const Name name = GetName();

    if (m_root.IsValid() && name.IsValid() && m_root->GetName() != name)
    {
        m_root->SetName(name);
    }
}

Handle<Node> Prefab::Spawn() const
{
    if (!m_root.IsValid())
    {
        HYP_LOG(Assets, Error, "Cannot spawn Prefab '{}' as it has no root node!", GetName());

        return Handle<Node>::Null();
    }

    Handle<Node> node = m_root->Clone();

    if (!node.IsValid())
    {
        HYP_LOG(Assets, Error, "Failed to clone root node of Prefab '{}'", GetName());

        return Handle<Node>::Null();
    }

    // Clone() carries the template's tags over, so drop any inherited source tag before applying ours
    TagAsPrefabInstance(node.Get(), GetUUID(), m_revision);

    if (GetName().IsValid())
    {
        node->SetName(GetName());
    }

    return node;
}

Handle<Node> Prefab::SpawnReplacementFor(const Node* instance) const
{
    Handle<Node> replacement = Spawn();

    if (!replacement.IsValid() || !instance)
    {
        return replacement;
    }

    replacement->SetName(instance->GetName());
    replacement->SetLocalTransform(instance->GetLocalTransform());
    replacement->SetUUID(instance->GetUUID());

    return replacement;
}

Handle<Prefab> Prefab::Find(const ANSIStringView& nameStr)
{
    return GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, StringHash(nameStr));
}

/// @TODO: Create an AssetRegistry method for this, and roll into that.
Handle<Prefab> Prefab::FindByUUID(const UUID& uuid)
{
    if (uuid == UUID::Invalid())
    {
        return Handle<Prefab>::Null();
    }

    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    Array<AssetDesc> descs;
    registry->GetBucketAssetDescs(AssetBuckets::Prefabs.GetIndex(), descs);

    for (const AssetDesc& desc : descs)
    {
        Handle<Prefab> prefab = registry->GetAsset<Prefab>(AssetBuckets::Prefabs, desc.name);

        if (prefab.IsValid() && prefab->GetUUID() == uuid)
        {
            return prefab;
        }
    }

    return Handle<Prefab>::Null();
}

Array<Handle<Node>> Prefab::FindLiveInstances(const World* world) const
{
    Array<Handle<Node>> instances;

    if (!world)
    {
        return instances;
    }

    const UUID prefabUUID = GetUUID();

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || (scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        const Handle<Node>& sceneRoot = scene->GetRoot();

        if (!sceneRoot.IsValid())
        {
            continue;
        }

        for (Node* node : sceneRoot->GetDescendants())
        {
            if (GetSourcePrefabUUID(node) == prefabUUID)
            {
                instances.PushBack(MakeStrongRef(node));
            }
        }
    }

    return instances;
}

UUID Prefab::GetSourcePrefabUUID(const Node* node)
{
    if (!node)
    {
        return UUID::Invalid();
    }

    if (const UUID* uuid = node->GetTag(s_namePrefabSource).data.TryGet<UUID>())
    {
        return *uuid;
    }

    return UUID::Invalid();
}

uint32 Prefab::GetInstanceRevision(const Node* node)
{
    if (!node)
    {
        return 0;
    }

    if (const int* revision = node->GetTag(s_namePrefabRevision).data.TryGet<int>())
    {
        return uint32(*revision);
    }

    return 0;
}

void Prefab::TagAsPrefabInstance(Node* node, const UUID& prefabUUID, uint32 revision)
{
    if (!node)
    {
        return;
    }

    // replace the existing tags, if any
    node->RemoveTag(s_namePrefabSource);
    node->RemoveTag(s_namePrefabRevision);

    node->AddTag(NodeTag(s_namePrefabSource, prefabUUID));

    if (revision != 0)
    {
        node->AddTag(NodeTag(s_namePrefabRevision, int(revision)));
    }
}

void Prefab::UntagAsPrefabInstance(Node* node)
{
    if (!node)
    {
        return;
    }

    node->RemoveTag(s_namePrefabSource);
    node->RemoveTag(s_namePrefabRevision);
}

uint32 Prefab::SyncStaleInstances(Node* root)
{
    if (!root)
    {
        return 0;
    }

    Map<UUID, Handle<Prefab>> prefabCache;
    Array<Pair<Handle<Node>, Handle<Prefab>>> stale;

    CollectStaleInstances(*root, prefabCache, stale);

    uint32 numReplaced = 0;

    for (const Pair<Handle<Node>, Handle<Prefab>>& entry : stale)
    {
        const Handle<Node>& instance = entry.first;
        Node* parent = instance->GetParent();

        Handle<Node> replacement = entry.second->SpawnReplacementFor(instance.Get());

        if (!parent || !replacement.IsValid())
        {
            continue;
        }

        instance->Remove();
        parent->AddChild(replacement);

        numReplaced++;
    }

    return numReplaced;
}

#pragma endregion Prefab

} // namespace Hyperion
