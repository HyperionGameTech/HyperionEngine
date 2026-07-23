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

void Prefab_OnPostLoad(Prefab& prefab)
{
    if (!EngineGlobals::IsShuttingDown())
    {
        const Handle<Node>& root = prefab.GetRoot();

        if (root.IsValid())
        {
            root->SetScene(GetDetachedSceneForThread(g_simThread));
        }
    }
}

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

} // namespace Hyperion
