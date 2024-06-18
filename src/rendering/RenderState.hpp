/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_STATE_HPP
#define HYPERION_RENDER_STATE_HPP

#include <core/containers/FlatSet.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/Stack.hpp>
#include <core/Defines.hpp>

#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawProxy.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <Types.hpp>

#include <stack>

namespace hyperion {

class RenderEnvironment;
class EnvGrid;

using RenderStateMask = uint32;

enum RenderStateMaskBits : RenderStateMask
{
    RENDER_STATE_NONE               = 0x0,
    RENDER_STATE_SCENE              = 0x1,
    RENDER_STATE_LIGHTS             = 0x2,
    RENDER_STATE_ENV_PROBES         = 0x4,
    RENDER_STATE_ACTIVE_ENV_PROBE   = 0x8,
    RENDER_STATE_CAMERA             = 0x10,
    RENDER_STATE_FRAME_COUNTER      = 0x20,

    RENDER_STATE_ALL                = 0xFFFFFFFFu
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
    const RenderList *render_list = nullptr;
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
    Stack<RenderBinding<Scene>>                                             scene_bindings;
    Stack<RenderBinding<Camera>>                                            camera_bindings;
    FlatMap<ID<Light>, LightDrawProxy>                                      lights;
    FixedArray<ArrayMap<ID<EnvProbe>, Optional<uint>>, ENV_PROBE_TYPE_MAX>  bound_env_probes; // map to texture slot
    ID<EnvGrid>                                                             bound_env_grid;
    Stack<ID<EnvProbe>>                                                     env_probe_bindings;
    uint32                                                                  frame_counter = ~0u;

    void AdvanceFrameCounter()
        { ++frame_counter; }

    void SetActiveEnvProbe(ID<EnvProbe> id)
        { env_probe_bindings.Push(id); }

    void UnsetActiveEnvProbe()
    {
        if (env_probe_bindings.Any()) {
            env_probe_bindings.Pop();
        }
    }

    ID<EnvProbe> GetActiveEnvProbe() const
        { return env_probe_bindings.Any() ? env_probe_bindings.Top() : ID<EnvProbe>(); }

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
            scene_bindings.Push(RenderBinding<Scene>::empty);
        } else {
            AssertThrow(scene->GetID().ToIndex() < max_scenes);

            scene_bindings.Push(RenderBinding<Scene> {
                scene->GetID(),
                scene->GetEnvironment(),
                &scene->GetRenderList(),
                scene->GetProxy()
            });
        }
    }

    void UnbindScene()
    {
        if (scene_bindings.Any()) {
            scene_bindings.Pop();
        }
    }

    const RenderBinding<Scene> &GetScene() const
    {
        return scene_bindings.Empty()
            ? RenderBinding<Scene>::empty
            : scene_bindings.Top();
    }

    void BindCamera(const Camera *camera)
    {
        if (camera == nullptr) {
            camera_bindings.Push(RenderBinding<Camera>::empty);
        } else {
            AssertThrow(camera->GetID().ToIndex() < max_cameras);

            camera_bindings.Push(RenderBinding<Camera> {
                camera->GetID(),
                camera->GetProxy()
            });
        }
    }

    void UnbindCamera()
    {
        if (camera_bindings.Any()) {
            camera_bindings.Pop();
        }
    }

    const RenderBinding<Camera> &GetCamera() const
    {
        return camera_bindings.Empty()
            ? RenderBinding<Camera>::empty
            : camera_bindings.Top();
    }

    void BindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
    {
        AssertThrow(type < ENV_PROBE_TYPE_MAX);

        constexpr EnvProbeBindingSlot binding_slots[ENV_PROBE_TYPE_MAX] = {
            ENV_PROBE_BINDING_SLOT_CUBEMAP,         // reflection
            ENV_PROBE_BINDING_SLOT_CUBEMAP,         // sky
            ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP,  // shadow
            ENV_PROBE_BINDING_SLOT_INVALID          // ambient
        };

        constexpr uint max_counts[ENV_PROBE_BINDING_SLOT_MAX] = {
            max_bound_reflection_probes,    // ENV_PROBE_BINDING_SLOT_CUBEMAP
            max_bound_point_shadow_maps     // ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP
        };

        const auto it = bound_env_probes[type].Find(probe_id);

        if (it != bound_env_probes[type].End()) {
            DebugLog(
                LogType::Info,
                "Probe #%u (type: %u) already bound, skipping.\n",
                probe_id.Value(),
                uint(type)
            );

            return;
        }

        const EnvProbeBindingSlot binding_slot = binding_slots[type];

        if (binding_slot != ENV_PROBE_BINDING_SLOT_INVALID) {
            if (m_env_probe_texture_slot_counters[uint(binding_slot)] >= max_counts[uint(binding_slot)]) {
                /*DebugLog(
                    LogType::Warn,
                    "Maximum bound probes of type %u exceeded! (%u)\n",
                    type,
                    max_counts[uint(binding_slot)]
                );*/

                return;
            }
        }

        DebugLog(
            LogType::Info,
            "Binding probe #%u (type: %u) to slot %u\n",
            probe_id.Value(),
            uint(type),
            binding_slot
        );

        bound_env_probes[type].Insert(
            probe_id,
            binding_slot != ENV_PROBE_BINDING_SLOT_INVALID
                ? Optional<uint>(m_env_probe_texture_slot_counters[uint(binding_slot)]++)
                : Optional<uint>()
        );
    }

    void UnbindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
    {
        // @FIXME: There's currently a bug where if an EnvProbe is unbound and then after several EnvProbes are bound, the
        // counter keeps increasing and the slot is never reused. This is because the counter is never reset.

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
            env_probe_bindings = { };
        }

        if (mask & RENDER_STATE_FRAME_COUNTER) {
            frame_counter = ~0u;
        }
    }

private:
    FixedArray<uint, ENV_PROBE_BINDING_SLOT_MAX>    m_env_probe_texture_slot_counters { };
};

} // namespace hyperion

#endif
