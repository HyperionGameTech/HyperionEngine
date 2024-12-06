/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderState.hpp>
#include <rendering/Camera.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>

#include <scene/camera/Camera.hpp>

namespace hyperion {

const RenderBinding<Scene> RenderBinding<Scene>::empty = { };

void RenderState::BindCamera(Camera *camera)
{
    AssertThrow(camera != nullptr);
    AssertThrow(camera->IsReady());
    
    camera_bindings.Push(&camera->GetRenderResources());
}

void RenderState::UnbindCamera(Camera *camera)
{
    AssertThrow(camera != nullptr);
    AssertThrow(camera->IsReady());

    AssertThrowMsg(camera_bindings.Any(), "No camera is currently bound!");
    AssertThrowMsg(camera_bindings.Top()->GetCamera() == camera, "Camera is not currently bound!");

    camera_bindings.Pop();
}

const CameraRenderResources &RenderState::GetActiveCamera() const
{
    static const CameraRenderResources empty { nullptr };

    return camera_bindings.Any()
        ? *camera_bindings.Top()
        : empty;
}

void RenderState::BindLight(Light *light)
{
    AssertThrow(light != nullptr);
    AssertThrow(light->IsReady());

    auto &array = bound_lights[uint32(light->GetLightType())];

    auto it = array.FindIf([light](const TResourceHandle<LightRenderResources> &item)
    {
        return item->GetLight() == light;
    });

    if (it != array.End()) {
        *it = TResourceHandle(light->GetRenderResources());
    } else {
        array.PushBack(TResourceHandle(light->GetRenderResources()));
    }
}

void RenderState::UnbindLight(Light *light)
{
    AssertThrow(light != nullptr);
    AssertThrow(light->IsReady());

    auto &array = bound_lights[uint32(light->GetLightType())];

    auto it = array.FindIf([light](const TResourceHandle<LightRenderResources> &item)
    {
        return item->GetLight() == light;
    });

    if (it != array.End()) {
        array.Erase(it);
    }
}

const TResourceHandle<LightRenderResources> &RenderState::GetActiveLight() const
{
    static const TResourceHandle<LightRenderResources> empty;

    return light_bindings.Any()
        ? light_bindings.Top()
        : empty;
}

void RenderState::SetActiveLight(LightRenderResources &light_render_resources)
{
    light_bindings.Push(TResourceHandle(light_render_resources));
}

void RenderState::BindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
{
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

void RenderState::UnbindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
{
    // @FIXME: There's currently a bug where if an EnvProbe is unbound and then after several EnvProbes are bound, the
    // counter keeps increasing and the slot is never reused. This is because the counter is never reset.

    AssertThrow(type < ENV_PROBE_TYPE_MAX);

    bound_env_probes[type].Erase(probe_id);
}

} // namespace hyperion