/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <PluginTest/PluginTestSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Framework/CVarManager.hpp>

#include <Plugins/PluginManager.hpp>

#include <PluginTestSystem.generated.inl>

namespace Hyperion {

static CVar<bool> s_cvPluginTestEnabled { "PluginTest.Enabled", true };

bool PluginTestSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void PluginTestSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (!s_cvPluginTestEnabled.Get())
    {
        return;
    }

    // the packaged-game check reads this from the log
    if (!m_hasLoggedFirstUpdate)
    {
        HYP_LOG(Plugins, Info, "PluginTestSystem first update ({} scene(s))", scenes.Size());

        m_hasLoggedFirstUpdate = true;
    }

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, testComponent] : scene->GetEntityManager()->GetEntitySet<PluginTestComponent>().GetScopedView(GetComponentInfos()))
        {
            testComponent.ticks++;
            m_totalTicks++;
        }
    }
}

} // namespace Hyperion
