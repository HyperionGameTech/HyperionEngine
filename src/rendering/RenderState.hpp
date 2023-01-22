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
#include <scene/camera/Camera.hpp>
#include <math/MathUtil.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

#include <stack>

namespace hyperion::v2 {

class RenderEnvironment;
class EnvGrid;

using RenderStateMask = UInt32;

enum RenderStateMaskBits : RenderStateMask
{
    RENDER_STATE_NONE = 0x0,
    RENDER_STATE_SCENE = 0x1,
    RENDER_STATE_LIGHTS = 0x2,
    RENDER_STATE_ENV_PROBES = 0x4,
    RENDER_STATE_ACTIVE_ENV_PROBE = 0x8,
    RENDER_STATE_VISIBILITY = 0x10,
    RENDER_STATE_CAMERA = 0x20,
    RENDER_STATE_FRAME_COUNTER = 0x40,

    RENDER_STATE_ALL = 0xFFFFFFFFu
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

template <>
struct RenderBinding<Camera>
{
    static const RenderBinding empty;

    ID<Camera> id;
    CameraDrawProxy camera;

    explicit operator bool() const { return bool(id); }
};

struct RenderState
{
    std::stack<RenderBinding<Scene>> scene_bindings;
    std::stack<RenderBinding<Camera>> camera_bindings;
    FlatMap<ID<Light>, LightDrawProxy> lights;
    FixedArray<FlatMap<ID<EnvProbe>, Optional<UInt>>, ENV_PROBE_TYPE_MAX> bound_env_probes; // map to texture slot
    ID<EnvGrid> bound_env_grid;
    ID<EnvProbe> current_env_probe; // For rendering to EnvProbe.
    UInt8 visibility_cursor = MathUtil::MaxSafeValue<UInt8>();
    UInt32 frame_counter = ~0u;

    void AdvanceFrameCounter()
        { ++frame_counter; }

    void SetActiveEnvProbe(ID<EnvProbe> id)
    {
        current_env_probe = id;
    }

    void UnsetActiveEnvProbe()
    {
        current_env_probe = ID<EnvProbe>();
    }

    void BindEnvGrid(ID<EnvGrid> id)
    {
        bound_env_grid = id;
    }

    void UnbindEnvGrid()
    {
        bound_env_grid = ID<EnvGrid>();
    }

    void BindLight(ID<Light> id, const LightDrawProxy &light)
    {
        lights.Insert(id, light);
    }

    void UnbindLight(ID<Light> id)
    {
        lights.Erase(id);
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

    void BindCamera(const Camera *camera)
    {
        if (camera == nullptr) {
            camera_bindings.push(RenderBinding<Camera>::empty);
        } else {
            camera_bindings.push(RenderBinding<Camera> {
                camera->GetID(),
                camera->GetDrawProxy()
            });
        }
    }

    void UnbindCamera()
    {
        camera_bindings.pop();
    }

    const RenderBinding<Camera> &GetCamera() const
    {
        return camera_bindings.empty()
            ? RenderBinding<Camera>::empty
            : camera_bindings.top();
    }

    void BindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
    {
        AssertThrow(type < ENV_PROBE_TYPE_MAX);

        constexpr UInt max_counts[ENV_PROBE_TYPE_MAX] = {
            max_bound_reflection_probes,
            max_bound_ambient_probes,
            max_bound_point_shadow_maps
        };

       const bool has_texture_slot = type != ENV_PROBE_TYPE_AMBIENT;

        if (has_texture_slot) {
            if (m_env_probe_texture_slot_counters[type] >= max_counts[type]) {
                DebugLog(
                    LogType::Warn,
                    "Maximum bound probes of type %u exceeded! (%u)\n",
                    type,
                    max_counts[type]
                );

                return;
            }
        }

        bound_env_probes[type].Insert(
            probe_id,
            has_texture_slot
                ? Optional<UInt>(m_env_probe_texture_slot_counters[type]++)
                : Optional<UInt>()
            );
    }

    void UnbindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
    {
        AssertThrow(type < ENV_PROBE_TYPE_MAX);

        bound_env_probes[type].Erase(probe_id);
    }

    void Reset(RenderStateMask mask)
    {
        if (mask & RENDER_STATE_ENV_PROBES) {
            for (auto &probes : bound_env_probes) {
                probes.Clear();
            }

            m_env_probe_texture_slot_counters = { };
        }

        if (mask & RENDER_STATE_SCENE) {
            scene_bindings = { };
        }

        if (mask & RENDER_STATE_CAMERA) {
            camera_bindings = { };
        }

        if (mask & RENDER_STATE_LIGHTS) {
            lights.Clear();
        }

        if (mask & RENDER_STATE_ACTIVE_ENV_PROBE) {
            UnsetActiveEnvProbe();
        }

        if (mask & RENDER_STATE_VISIBILITY) {
            visibility_cursor = 0u;
        }

        if (mask & RENDER_STATE_FRAME_COUNTER) {
            frame_counter = ~0u;
        }
    }

private:
    FixedArray<UInt, ENV_PROBE_TYPE_MAX> m_env_probe_texture_slot_counters { };
};

} // namespace hyperion::v2

#endif