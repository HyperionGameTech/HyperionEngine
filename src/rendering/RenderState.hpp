#ifndef HYPERION_V2_RENDER_STATE_HPP
#define HYPERION_V2_RENDER_STATE_HPP

#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawProxy.hpp>
#include <scene/Scene.hpp>
#include <math/MathUtil.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

#include <stack>

namespace hyperion::v2 {

class RenderEnvironment;

using RenderStateMask = UInt32;

enum RenderStateMaskBits : RenderStateMask
{
    RENDER_STATE_NONE = 0x0,
    RENDER_STATE_SCENE = 0x1,
    RENDER_STATE_LIGHTS = 0x2,
    RENDER_STATE_ENV_PROBES = 0x4,
    RENDER_STATE_VISIBILITY = 0x8,

    RENDER_STATE_ALL = 0xFFFFFFFF
};


// Basic render side binding, by default holding only ID of an object.
template <class T>
struct RenderBinding
{
    static const RenderBinding empty;

    ID<T> id;

    explicit operator bool() const { return bool(id); }

    explicit operator ID<T>() const
        { return id; }

    bool operator==(const RenderBinding &other) const
        { return id == other.id; }

    bool operator<(const RenderBinding &other) const
        { return id < other.id; }

    bool operator==(ID<T> id) const
        { return this->id == id; }
};

template <class T>
const RenderBinding<T> RenderBinding<T>::empty = RenderBinding { };

template <>
struct RenderBinding<Scene>
{
    static const RenderBinding empty;

    ID<Scene> id;
    RenderEnvironment *render_environment = nullptr;
    SceneDrawProxy scene;

    explicit operator bool() const { return bool(id); }
};

struct RenderState
{
    std::stack<RenderBinding<Scene>> scene_bindings;
    FlatSet<RenderBinding<Light>> light_bindings;
    FlatMap<ID<EnvProbe>, Optional<UInt>> env_probes; // map to texture slot
    UInt8 visibility_cursor = MathUtil::MaxSafeValue<UInt8>();

    void BindLight(ID<Light> light_id)
    {
        light_bindings.Insert(RenderBinding<Light> { light_id });
    }

    void UnbindLight(ID<Light> light_id)
    {
        light_bindings.Erase(RenderBinding<Light> { light_id });
    }

    void BindScene(const Scene *scene)
    {
        if (scene == nullptr) {
            scene_bindings.push(RenderBinding<Scene>::empty);
        } else {
            scene_bindings.push(RenderBinding<Scene> {
                scene->GetID(),
                scene->GetEnvironment(),
                scene->GetDrawProxy()
            });
        }
    }

    void UnbindScene()
    {
        scene_bindings.pop();
    }

    const RenderBinding<Scene> &GetScene() const
    {
        return scene_bindings.empty()
            ? RenderBinding<Scene>::empty
            : scene_bindings.top();
    }

    void BindEnvProbe(ID<EnvProbe> env_probe)
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

    void UnbindEnvProbe(ID<EnvProbe> env_probe)
    {
        env_probes.Erase(env_probe);
    }

    void Reset(RenderStateMask mask)
    {
        if (mask & RENDER_STATE_ENV_PROBES) {
            env_probes.Clear();
            m_env_probe_texture_slot_counter = 0u;
        }

        if (mask & RENDER_STATE_SCENE) {
            scene_bindings = {};
        }

        if (mask & RENDER_STATE_LIGHTS) {
            light_bindings.Clear();
        }

        if (mask & RENDER_STATE_VISIBILITY) {
            visibility_cursor = 0u;
        }
    }

private:
    UInt m_env_probe_texture_slot_counter = 0u;
};

} // namespace hyperion::v2

#endif