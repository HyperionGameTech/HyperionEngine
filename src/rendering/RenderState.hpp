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
class RenderEnvGrid;
class Camera;
class RenderCamera;

using RenderStateMask = uint32;

enum RenderStateMaskBits : RenderStateMask
{
    RENDER_STATE_NONE = 0x0,
    RENDER_STATE_SCENE = 0x1,
    RENDER_STATE_LIGHTS = 0x2,
    RENDER_STATE_ACTIVE_LIGHT = 0x4,
    RENDER_STATE_CAMERA = 0x20,

    RENDER_STATE_ALL = 0xFFFFFFFFu
};

HYP_CLASS()
class RenderState final : public HypObject<RenderState>
{
    HYP_OBJECT_BODY(RenderState);

public:
    Stack<TResourceHandle<RenderScene>> scene_bindings;
    Array<TResourceHandle<RenderCamera>> camera_bindings;
    Stack<TResourceHandle<RenderLight>> light_bindings;

    HYP_API RenderState();
    RenderState(const RenderState&) = delete;
    RenderState& operator=(const RenderState&) = delete;
    RenderState(RenderState&&) noexcept = delete;
    RenderState& operator=(RenderState&&) noexcept = delete;
    HYP_API ~RenderState();

    void SetActiveLight(const TResourceHandle<RenderLight>& light_resource_handle);

    HYP_FORCE_INLINE void UnsetActiveLight()
    {
        if (light_bindings.Any())
        {
            light_bindings.Pop();
        }
    }

    const TResourceHandle<RenderLight>& GetActiveLight() const;

    HYP_FORCE_INLINE RenderScene* GetActiveScene() const
    {
        return scene_bindings.Empty()
            ? nullptr
            : scene_bindings.Top().Get();
    }

    HYP_FORCE_INLINE void SetActiveScene(const Scene* scene)
    {
        if (scene == nullptr)
        {
            scene_bindings.Push(TResourceHandle<RenderScene>());
        }
        else
        {
            scene_bindings.Push(TResourceHandle<RenderScene>(scene->GetRenderResource()));
        }
    }

    HYP_FORCE_INLINE void UnsetActiveScene()
    {
        if (scene_bindings.Any())
        {
            scene_bindings.Pop();
        }
    }

    void BindCamera(const TResourceHandle<RenderCamera>& resource_handle);
    void UnbindCamera(const RenderCamera* render_camera);

    const TResourceHandle<RenderCamera>& GetActiveCamera() const;

    void BindEnvProbe(EnvProbeType type, TResourceHandle<RenderEnvProbe>&& resource_handle);
    void UnbindEnvProbe(EnvProbeType type, RenderEnvProbe* env_render_probe);

    void ResetStates(RenderStateMask mask)
    {
        if (mask & RENDER_STATE_SCENE)
        {
            scene_bindings = {};
        }

        if (mask & RENDER_STATE_CAMERA)
        {
            camera_bindings = {};
        }

        if (mask & RENDER_STATE_ACTIVE_LIGHT)
        {
            light_bindings = {};
        }
    }

private:
    HYP_API void Init() override;

    FixedArray<uint32, ENV_PROBE_BINDING_SLOT_MAX> m_env_probe_texture_slot_counters {};
};

} // namespace hyperion

#endif
