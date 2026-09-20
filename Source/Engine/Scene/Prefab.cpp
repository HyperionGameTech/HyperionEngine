/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Node.hpp>
#include <Scene/DetachedScene.hpp>

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
    MarkDirty();
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
    node->RemoveTag(s_namePrefabSource);
    node->AddTag(NodeTag(s_namePrefabSource, GetUUID()));

    return node;
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

#pragma endregion Prefab

} // namespace Hyperion
