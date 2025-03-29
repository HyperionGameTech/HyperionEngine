/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_STATE_HPP
#define HYPERION_RENDER_STATE_HPP

#include <core/utilities/Optional.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/Stack.hpp>

#include <core/object/HypObject.hpp>
#include <core/Base.hpp>

#include <core/Defines.hpp>

#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/RenderScene.hpp>

#include <scene/Scene.hpp>
#include <scene/Light.hpp> // For LightType
#include <scene/EnvProbe.hpp>

#include <Types.hpp>

namespace hyperion {

class RenderEnvironment;
class EnvGrid;
class Camera;
class CameraRenderResource;

using RenderStateMask = uint32;

enum RenderStateMaskBits : RenderStateMask
{
    RENDER_STATE_NONE               = 0x0,
    RENDER_STATE_SCENE              = 0x1,
    RENDER_STATE_LIGHTS             = 0x2,
    RENDER_STATE_ACTIVE_LIGHT       = 0x4,
    RENDER_STATE_ENV_PROBES         = 0x8,
    RENDER_STATE_ACTIVE_ENV_PROBE   = 0x10,
    RENDER_STATE_CAMERA             = 0x20,
    RENDER_STATE_FRAME_COUNTER      = 0x40,

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

    ID<Scene>                   id;
    Handle<RenderEnvironment>   render_environment;
    const RenderCollector       *render_collector = nullptr;
    SceneDrawProxy              scene;

    HYP_FORCE_INLINE explicit operator bool() const
        { return bool(id); }
};

template <class RenderResourceType>
struct RenderResourceState_StackContext
{
    Stack<TResourceHandle<RenderResourceType>>  stack;

    const TResourceHandle<RenderResourceType> &Current()
    {
        static const TResourceHandle<RenderResourceType> empty;

        if (stack.Empty()) {
            return empty;
        }

        return stack.Top();
    }

    void Bind(RenderResourceType &resource)
    {
        stack.Push(TResourceHandle<RenderResourceType>(resource));
    }

    void Unbind()
    {
        stack.Pop();
    }
};

template <class RenderResourceType, SizeType MaxSize = SizeType(-1)>
struct RenderResourceState_PoolContext
{
    HashSet<RenderResourceType *>   elements;

    void Enable(RenderResourceType *resource)
    {
        if (!resource) {
            return;
        }

        elements.Insert(resource);
    }

    void Disable(RenderResourceType *resource)
    {
        elements.Erase(resource);
    }
};

template <class RenderResourceType>
struct RenderResourceState;

template <>
struct RenderResourceState<SceneRenderResource>
    : RenderResourceState_StackContext<SceneRenderResource>,
      RenderResourceState_PoolContext<SceneRenderResource>
{
};

// @TODO: Refactor
HYP_CLASS()
class RenderState : public HypObject<RenderState>
{
    HYP_OBJECT_BODY(RenderState);

public:
    Stack<TResourceHandle<SceneRenderResource>>                                     scene_bindings;
    Array<CameraRenderResource *>                                                   camera_bindings;
    FixedArray<Array<TResourceHandle<LightRenderResource>>, uint32(LightType::MAX)> bound_lights;
    Stack<TResourceHandle<LightRenderResource>>                                     light_bindings;
    Array<EnvGrid *>                                                                env_grid_bindings;
    Stack<TResourceHandle<EnvProbeRenderResource>>                                  env_probe_bindings;
    FixedArray<Array<TResourceHandle<EnvProbeRenderResource>>, ENV_PROBE_TYPE_MAX>  bound_env_probes;
    
    uint32                                                                          frame_counter = ~0u;

    HYP_API RenderState();
    RenderState(const RenderState &)                = delete;
    RenderState &operator=(const RenderState &)     = delete;
    RenderState(RenderState &&) noexcept            = delete;
    RenderState &operator=(RenderState &&) noexcept = delete;
    HYP_API ~RenderState();

    HYP_API void Init();

    HYP_FORCE_INLINE void AdvanceFrameCounter()
        { ++frame_counter; }

    HYP_FORCE_INLINE void SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource> &&resource_handle)
        { env_probe_bindings.Push(std::move(resource_handle)); }

    HYP_FORCE_INLINE void UnsetActiveEnvProbe()
    {
        if (env_probe_bindings.Any()) {
            env_probe_bindings.Pop();
        }
    }

    const TResourceHandle<EnvProbeRenderResource> &GetActiveEnvProbe() const;

    HYP_FORCE_INLINE void BindEnvGrid(EnvGrid *env_grid)
    {
        env_grid_bindings.PushBack(env_grid);
    }

    HYP_FORCE_INLINE void UnbindEnvGrid(EnvGrid *env_grid)
    {
        for (auto it = env_grid_bindings.Begin(); it != env_grid_bindings.End(); ++it) {
            if (*it == env_grid) {
                it = env_grid_bindings.Erase(it);
            } else {
                ++it;
            }
        }
    }

    HYP_FORCE_INLINE EnvGrid *GetActiveEnvGrid() const
    {
        return env_grid_bindings.Empty()
            ? nullptr
            : env_grid_bindings.Back();
    }

    HYP_FORCE_INLINE uint32 NumBoundLights() const
    {
        uint32 count = 0;

        for (uint32 i = 0; i < uint32(LightType::MAX); i++) {
            count += uint32(bound_lights[i].Size());
        }

        return count;
    }

    void BindLight(Light *light);
    void UnbindLight(Light *light);

    void SetActiveLight(LightRenderResource &light_render_resource);

    HYP_FORCE_INLINE void UnsetActiveLight()
    {
        if (light_bindings.Any()) {
            light_bindings.Pop();
        }
    }

    const TResourceHandle<LightRenderResource> &GetActiveLight() const;

    HYP_FORCE_INLINE SceneRenderResource *GetActiveScene() const
    {
        return scene_bindings.Empty()
            ? nullptr
            : scene_bindings.Top().Get();
    }

    HYP_FORCE_INLINE void SetActiveScene(const Scene *scene)
    {
        if (scene == nullptr) {
            scene_bindings.Push(TResourceHandle<SceneRenderResource>());
        } else {
            scene_bindings.Push(TResourceHandle<SceneRenderResource>(scene->GetRenderResource()));
        }
    }

    HYP_FORCE_INLINE void UnsetActiveScene()
    {
        if (scene_bindings.Any()) {
            scene_bindings.Pop();
        }
    }

    void BindCamera(Camera *camera);
    void UnbindCamera(Camera *camera);

    const CameraRenderResource &GetActiveCamera() const;

    void BindEnvProbe(EnvProbeType type, TResourceHandle<EnvProbeRenderResource> &&resource_handle);
    void UnbindEnvProbe(EnvProbeType type, EnvProbeRenderResource *env_probe_render_resource);

    void ResetStates(RenderStateMask mask)
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
            for (auto &lights : bound_lights) {
                lights.Clear();
            }
        }

        if (mask & RENDER_STATE_ACTIVE_LIGHT) {
            light_bindings = { };
        }

        if (mask & RENDER_STATE_ACTIVE_ENV_PROBE) {
            env_probe_bindings = { };
        }

        if (mask & RENDER_STATE_FRAME_COUNTER) {
            frame_counter = ~0u;
        }
    }

private:
    FixedArray<uint32, ENV_PROBE_BINDING_SLOT_MAX>    m_env_probe_texture_slot_counters { };
};

} // namespace hyperion

#endif
