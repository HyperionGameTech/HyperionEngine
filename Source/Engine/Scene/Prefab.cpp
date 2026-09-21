/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Node.hpp>
#include <Scene/DetachedScene.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Prefab.generated.inl>

namespace Hyperion {

static const Name s_namePrefabSource = NAME("PrefabSource");

void Prefab_OnPostLoad(Prefab& prefab)
{
    if (!EngineGlobals::IsShuttingDown())
    {
        const Handle<Node>& root = prefab.GetRoot();

        if (root.IsValid())
        {
            // Older prefabs may have a root named differently from the asset (e.g. a suffixed name on save)
            prefab.SyncRootName();

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
    SyncRootName();

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
        HYP_LOG(Assets, Warning, "Cannot spawn Prefab '{}': it has no root node", GetName());

        return Handle<Node>::Null();
    }

    Handle<Node> node = m_root->Clone();

    if (!node.IsValid())
    {
        HYP_LOG(Assets, Error, "Failed to clone root node of Prefab '{}'", GetName());

        return Handle<Node>::Null();
    }

    // Clone() carries the template's tags over, so drop any inherited source tag before applying ours
    TagAsPrefabInstance(node.Get(), GetUUID());

    if (GetName().IsValid())
    {
        node->SetName(GetName());
    }

    return node;
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

void Prefab::TagAsPrefabInstance(Node* node, const UUID& prefabUUID)
{
    if (!node)
    {
        return;
    }

    // A node may already carry a source tag (e.g. it was cloned from another instance), so
    // replace it outright rather than stacking tags.
    node->RemoveTag(s_namePrefabSource);
    node->AddTag(NodeTag(s_namePrefabSource, prefabUUID));
}

void Prefab::UntagAsPrefabInstance(Node* node)
{
    if (!node)
    {
        return;
    }

    node->RemoveTag(s_namePrefabSource);
}

#pragma endregion Prefab

} // namespace Hyperion
