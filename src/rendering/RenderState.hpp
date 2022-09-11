#ifndef HYPERION_V2_RENDER_STATE_HPP
#define HYPERION_V2_RENDER_STATE_HPP

#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/IndirectDraw.hpp>
#include <scene/Scene.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

#include <stack>

namespace hyperion::v2 {

struct RenderState
{
    struct SceneBinding
    {
        Scene::ID id;

        explicit operator bool() const { return bool(id); }
    };

    std::stack<SceneBinding> scene_ids;
    FlatSet<Light::ID> light_ids;
    FlatMap<EnvProbe::ID, Optional<UInt>> env_probes; // map to texture slot
    UInt8 visibility_cursor = 0u;

    void BindLight(Light::ID light)
    {
        light_ids.Insert(light);
    }

    void UnbindLight(Light::ID light)
    {
        light_ids.Erase(light);
    }

    void BindScenes(Scene::ID scene_id)
    {
        scene_ids.push(SceneBinding { scene_id });
    }

    void BindScene(const Scene *scene)
    {
        scene_ids.push(
            scene == nullptr
                ? SceneBinding { }
                : SceneBinding { scene->GetID() }
        );
    }

    void UnbindScene()
    {
        scene_ids.pop();
    }

    SceneBinding GetScene() const
    {
        return scene_ids.empty() ? SceneBinding { } : scene_ids.top();
    }

    void BindEnvProbe(EnvProbe::ID env_probe)
    {
        if (m_env_probe_texture_slot_counter >= max_bound_env_probes) {
            DebugLog(
                LogType::Warn,
                "Maximum bound env probes (%u) exceeded!\n",
                max_bound_env_probes
            );

            return;
        }

        env_probes.Insert(env_probe, Optional<UInt>(m_env_probe_texture_slot_counter++));
    }

    void Reset()
    {
        env_probes.Clear();
        m_env_probe_texture_slot_counter = 0u;

        scene_ids = {};

        light_ids.Clear();

        visibility_cursor = 0u;
    }

private:
    UInt m_env_probe_texture_slot_counter = 0u;
};

} // namespace hyperion::v2

#endif