/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderProbe.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(RenderState, Rendering);

const RenderBinding<Scene> RenderBinding<Scene>::empty = { };

RenderState::RenderState()
{
}

RenderState::~RenderState() = default;

void RenderState::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    static const struct DefaultCameraInitializer
    {
        Handle<Camera>  camera;

        DefaultCameraInitializer()
        {
            camera = CreateObject<Camera>();
            camera->SetName(NAME("RenderState_DefaultCamera"));
            InitObject(camera);
        }
    } default_camera_initializer;

    // Ensure the default camera is always set
    camera_bindings.PushBack(&default_camera_initializer.camera->GetRenderResource());

    SetReady(true);
}

void RenderState::UnbindScene(const Scene *scene)
{
    AssertThrow(scene != nullptr);
    AssertThrow(scene->IsReady());

    Threads::AssertOnThread(g_render_thread);

    SceneRenderResource *scene_render_resources = &scene->GetRenderResource();

    for (auto it = scene_bindings.Begin(); it != scene_bindings.End();) {
        if ((*it) == scene_render_resources) {
            it = scene_bindings.Erase(it);

            // Stop iterating at first match
            break;
        } else {
            ++it;
        }
    }
}

void RenderState::BindCamera(Camera *camera)
{
    AssertThrow(camera != nullptr);
    AssertThrow(camera->IsReady());

    Threads::AssertOnThread(g_render_thread);
    
    // Allow multiple of the same so we can always override the topmost camera
    camera_bindings.PushBack(&camera->GetRenderResource());
}

void RenderState::UnbindCamera(Camera *camera)
{
    AssertThrow(camera != nullptr);
    AssertThrow(camera->IsReady());

    Threads::AssertOnThread(g_render_thread);

    CameraRenderResource *camera_render_resources = &camera->GetRenderResource();

    for (auto it = camera_bindings.Begin(); it != camera_bindings.End();) {
        if ((*it) == camera_render_resources) {
            it = camera_bindings.Erase(it);

            // Stop iterating at first match
            break;
        } else {
            ++it;
        }
    }
}

const CameraRenderResource &RenderState::GetActiveCamera() const
{
    Threads::AssertOnThread(g_render_thread);

    static const CameraRenderResource empty { nullptr };

    return camera_bindings.Any()
        ? *camera_bindings.Back()
        : empty;
}

void RenderState::BindLight(Light *light)
{
    AssertThrow(light != nullptr);
    AssertThrow(light->IsReady());

    Threads::AssertOnThread(g_render_thread);

    auto &array = bound_lights[uint32(light->GetLightType())];

    auto it = array.FindIf([light](const TResourceHandle<LightRenderResource> &item)
    {
        return item->GetLight() == light;
    });

    if (it != array.End()) {
        *it = TResourceHandle(light->GetRenderResource());
    } else {
        array.PushBack(TResourceHandle(light->GetRenderResource()));
    }
}

void RenderState::UnbindLight(Light *light)
{
    AssertThrow(light != nullptr);
    AssertThrow(light->IsReady());

    Threads::AssertOnThread(g_render_thread);

    auto &array = bound_lights[uint32(light->GetLightType())];

    auto it = array.FindIf([light](const TResourceHandle<LightRenderResource> &item)
    {
        return item->GetLight() == light;
    });

    if (it != array.End()) {
        array.Erase(it);
    }
}

const TResourceHandle<LightRenderResource> &RenderState::GetActiveLight() const
{
    Threads::AssertOnThread(g_render_thread);

    static const TResourceHandle<LightRenderResource> empty;

    return light_bindings.Any()
        ? light_bindings.Top()
        : empty;
}

void RenderState::SetActiveLight(LightRenderResource &light_render_resources)
{
    Threads::AssertOnThread(g_render_thread);

    light_bindings.Push(TResourceHandle(light_render_resources));
}

void RenderState::BindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
{
    Threads::AssertOnThread(g_render_thread);

    constexpr EnvProbeBindingSlot binding_slots[ENV_PROBE_TYPE_MAX] = {
        ENV_PROBE_BINDING_SLOT_CUBEMAP,         // reflection
        ENV_PROBE_BINDING_SLOT_CUBEMAP,         // sky
        ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP,  // shadow
        ENV_PROBE_BINDING_SLOT_INVALID          // ambient
    };

    constexpr uint32 max_counts[ENV_PROBE_BINDING_SLOT_MAX] = {
        max_bound_reflection_probes,    // ENV_PROBE_BINDING_SLOT_CUBEMAP
        max_bound_point_shadow_maps     // ENV_PROBE_BINDING_SLOT_SHADOW_CUBEMAP
    };

    const auto it = bound_env_probes[type].Find(probe_id);

    if (it != bound_env_probes[type].End()) {
        DebugLog(
            LogType::Info,
            "Probe #%u (type: %u) already bound, skipping.\n",
            probe_id.Value(),
            uint32(type)
        );

        return;
    }

    const EnvProbeBindingSlot binding_slot = binding_slots[type];

    if (binding_slot != ENV_PROBE_BINDING_SLOT_INVALID) {
        if (m_env_probe_texture_slot_counters[uint32(binding_slot)] >= max_counts[uint32(binding_slot)]) {
            /*DebugLog(
                LogType::Warn,
                "Maximum bound probes of type %u exceeded! (%u)\n",
                type,
                max_counts[uint32(binding_slot)]
            );*/

            return;
        }
    }

    bound_env_probes[type].Insert(
        probe_id,
        binding_slot != ENV_PROBE_BINDING_SLOT_INVALID
            ? Optional<uint32>(m_env_probe_texture_slot_counters[uint32(binding_slot)]++)
            : Optional<uint32>()
    );
}

void RenderState::UnbindEnvProbe(EnvProbeType type, ID<EnvProbe> probe_id)
{
    Threads::AssertOnThread(g_render_thread);

    // @FIXME: There's currently a bug where if an EnvProbe is unbound and then after several EnvProbes are bound, the
    // counter keeps increasing and the slot is never reused. This is because the counter is never reset.

    AssertThrow(type < ENV_PROBE_TYPE_MAX);

    bound_env_probes[type].Erase(probe_id);
}

} // namespace hyperion