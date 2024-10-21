/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_STATE_HPP
#define HYPERION_RENDER_STATE_HPP

#include <core/containers/FlatMap.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/Stack.hpp>
#include <core/Defines.hpp>

#include <rendering/Light.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawProxy.hpp>

#include <scene/Scene.hpp>

#include <Types.hpp>

namespace hyperion {

class RenderEnvironment;
class EnvGrid;
class Camera;

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

    HYP_FORCE_INLINE explicit operator bool() const
        { return bool(id); }

    HYP_FORCE_INLINE explicit operator ID<T>() const
        { return id; }

    HYP_FORCE_INLINE bool operator==(const RenderBinding &other) const
        { return id == other.id; }

    HYP_FORCE_INLINE bool operator<(const RenderBinding &other) const
        { return id < other.id; }

    HYP_FORCE_INLINE bool operator==(ID<T> id) const
        { return this->id == id; }
};

template <class T>
const RenderBinding<T> RenderBinding<T>::empty = RenderBinding { };

template <>
struct RenderBinding<Scene>
{
    static const RenderBinding empty;

    ID<Scene>           id;
    RenderEnvironment   *render_environment = nullptr;
    const RenderCollector    *render_collector = nullptr;
    SceneDrawProxy      scene;

    HYP_FORCE_INLINE explicit operator bool() const
        { return bool(id); }
};

template <>
struct RenderBinding<Camera>
{
    static const RenderBinding empty;

    ID<Camera>      id;
    CameraDrawProxy camera;

    HYP_FORCE_INLINE explicit operator bool() const
        { return bool(id); }
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

    HYP_FORCE_INLINE void AdvanceFrameCounter()
        { ++frame_counter; }

    HYP_FORCE_INLINE void SetActiveEnvProbe(ID<EnvProbe> id)
        { env_probe_bindings.Push(id); }

    HYP_FORCE_INLINE void UnsetActiveEnvProbe()
    {
        if (env_probe_bindings.Any()) {
            env_probe_bindings.Pop();
        }
    }

    HYP_FORCE_INLINE ID<EnvProbe> GetActiveEnvProbe() const
        { return env_probe_bindings.Any() ? env_probe_bindings.Top() : ID<EnvProbe>(); }

    HYP_FORCE_INLINE void BindEnvGrid(ID<EnvGrid> id)
    {
        bound_env_grid = id;
    }

    HYP_FORCE_INLINE void UnbindEnvGrid()
    {
        bound_env_grid = ID<EnvGrid>();
    }

    HYP_FORCE_INLINE void BindLight(ID<Light> id, const LightDrawProxy &light)
    {
        lights.Insert(id, light);
    }

    HYP_FORCE_INLINE void UnbindLight(ID<Light> id)
    {
        lights.Erase(id);
    }

    HYP_FORCE_INLINE void BindScene(const Scene *scene)
    {
        if (scene == nullptr) {
            scene_bindings.Push(RenderBinding<Scene>::empty);
        } else {
            AssertThrow(scene->GetID().ToIndex() < max_scenes);

            scene_bindings.Push(RenderBinding<Scene> {
                scene->GetID(),
                scene->GetEnvironment(),
                &scene->GetRenderCollector(),
                scene->GetProxy()
            });
        }
    }

    HYP_FORCE_INLINE void UnbindScene()
    {
        if (scene_bindings.Any()) {
            scene_bindings.Pop();
        }
    }

    HYP_FORCE_INLINE const RenderBinding<Scene> &GetScene() const
    {
        return scene_bindings.Empty()
            ? RenderBinding<Scene>::empty
            : scene_bindings.Top();
    }

    void BindCamera(const Camera *camera);
    void UnbindCamera();

    HYP_FORCE_INLINE const RenderBinding<Camera> &GetCamera() const
    {
        return camera_bindings.Empty()
            ? RenderBinding<Camera>::empty
            : camera_bindings.Top();
    }

    void BindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id);
    void UnbindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id);

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
