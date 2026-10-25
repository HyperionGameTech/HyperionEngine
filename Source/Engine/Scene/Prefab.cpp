/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/DetachedScene.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/Mesh.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Prefab.generated.inl>

namespace Hyperion {

static const Name s_namePrefabSource = NAME("PrefabSource");
static const Name s_namePrefabRevision = NAME("PrefabRevision");

Delegate<void, Prefab*> Prefab::OnPrefabChanged;

namespace {
    
Handle<Entity> EnsureEntityRoot(const Handle<Node>& node)
{
    if (!node.IsValid())
    {
        return Handle<Entity>::Null();
    }

    if (Handle<Entity> entity = TryCast<Entity>(node); entity.IsValid())
    {
        return entity;
    }

    Handle<Entity> converted = TryCast<Entity>(node->CloneAs(Entity::StaticClass(), /* cloneChildren */ true));

    if (!converted.IsValid())
    {
        HYP_LOG(Assets, Error, "Failed to convert prefab root '{}' to an Entity", node->GetName());

        return Handle<Entity>::Null();
    }

    converted->SetUUID(node->GetUUID());

    return converted;
}

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

void Prefab_OnPostLoad(Prefab& prefab)
{
    if (!EngineGlobals::IsShuttingDown())
    {
        // Prefabs previously had Node root, convert it on load.
        if (prefab.m_root.IsValid() && !prefab.m_root->IsA<Entity>())
        {
            prefab.m_root = EnsureEntityRoot(prefab.m_root);
        }

        const Handle<Node>& root = prefab.m_root;

        if (root.IsValid())
        {
            prefab.SyncRootName();

            if (Handle<Entity> rootEntity = prefab.GetRoot(); rootEntity.IsValid() && !rootEntity->GetEntityManager())
            {
                GetDetachedSceneForCurrentThread().GetEntityManager()->AddExistingEntity(rootEntity);
            }

            root->SetScene(&GetDetachedSceneForThread(g_simThread));
        }
    }
}

#pragma region Prefab

Prefab::Prefab()
    : Prefab(Name::Invalid())
{
}

Prefab::Prefab(Name name, const Handle<Node>& root)
    : AssetObject(name),
      m_root(EnsureEntityRoot(root))
{
}

Result Prefab::Rename(Name name)
{
    Result result = AssetObject::Rename(name);

    SyncRootName();

    return result;
}

Handle<Entity> Prefab::GetRoot() const
{
    return TryCast<Entity>(m_root);
}

void Prefab::SetRoot(const Handle<Node>& root)
{
    if (root == m_root)
    {
        return;
    }

    m_root = EnsureEntityRoot(root);
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

uint32 Prefab::FindLargestMesh(Handle<Mesh>& outMesh) const
{
    outMesh = Handle<Mesh>::Null();

    Set<const Mesh*> distinctMeshes;
    float largestSurfaceArea = -1.0f;

    auto visitNode = [&](Node* node)
    {
        Entity* entity = TryCast<Entity>(node);
        MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (!meshComponent || !meshComponent->mesh.IsValid())
        {
            return;
        }

        distinctMeshes.Insert(meshComponent->mesh.Get());

        const BoundingBox worldBounds = node->GetWorldMatrix() * meshComponent->mesh->GetAABB();

        if (!worldBounds.IsValid() || !worldBounds.IsFinite())
        {
            return;
        }

        const float surfaceArea = worldBounds.GetSurfaceArea();

        if (surfaceArea > largestSurfaceArea)
        {
            largestSurfaceArea = surfaceArea;
            outMesh = meshComponent->mesh;
        }
    };

    if (Node* root = m_root.Get())
    {
        visitNode(root);

        for (Node* descendant : root->GetDescendants())
        {
            visitNode(descendant);
        }
    }

    return uint32(distinctMeshes.Size());
}

Handle<Mesh> Prefab::GetLargestMesh() const
{
    Handle<Mesh> mesh;
    FindLargestMesh(mesh);

    return mesh;
}

uint32 Prefab::CountMeshes() const
{
    Handle<Mesh> mesh;

    return FindLargestMesh(mesh);
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

    if (const UUID* uuid = node->GetDataEntry(s_namePrefabSource).data.TryGet<UUID>())
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

    if (const int* revision = node->GetDataEntry(s_namePrefabRevision).data.TryGet<int>())
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
    node->RemoveDataEntry(s_namePrefabSource);
    node->RemoveDataEntry(s_namePrefabRevision);

    node->AddDataEntry(NodeData(s_namePrefabSource, prefabUUID));

    if (revision != 0)
    {
        node->AddDataEntry(NodeData(s_namePrefabRevision, int(revision)));
    }
}

void Prefab::UntagAsPrefabInstance(Node* node)
{
    if (!node)
    {
        return;
    }

    node->RemoveDataEntry(s_namePrefabSource);
    node->RemoveDataEntry(s_namePrefabRevision);
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
