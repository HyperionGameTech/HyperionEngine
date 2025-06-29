#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/PlaceholderData.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/Texture.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Defines.hpp>

#include <Engine.hpp>

namespace hyperion {

void OnReflectionProbeBindingChanged(EnvProbe* env_probe, uint32 prev, uint32 next)
{
    AssertDebug(env_probe != nullptr);
    AssertDebug(env_probe->IsReady());

    AssertThrow(env_probe->IsInstanceOf(SkyProbe::Class()) || env_probe->IsInstanceOf(ReflectionProbe::Class()));

    if (!env_probe->GetPrefilteredEnvMap().IsValid())
    {
        HYP_LOG(Rendering, Error, "EnvProbe {} (class: {}) has no prefiltered env map set!\n", env_probe->GetID(),
            env_probe->InstanceClass()->GetName());

        return;
    }

    DebugLog(LogType::Debug, "EnvProbe %u (class: %s) binding changed from %u to %u\n", env_probe->GetID().Value(),
        *env_probe->InstanceClass()->GetName(),
        prev, next);

    if (prev != ~0u)
    {
        HYP_LOG(Rendering, Debug, "UN setting env probe texture at index: {}", prev);
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), prev, g_render_global_state->PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }
    }
    else
    {
        env_probe->GetRenderResource().IncRef();
        env_probe->GetPrefilteredEnvMap()->GetRenderResource().IncRef();
    }

    IRenderProxy* proxy = RendererAPI_GetRenderProxy(env_probe->GetID());
    AssertThrow(proxy != nullptr);

    RenderProxyEnvProbe* proxy_casted = static_cast<RenderProxyEnvProbe*>(proxy);

    // temp shit
    // proxy_casted->bound_index = next;
    AssertDebug(env_probe->GetRenderResource().GetBufferIndex() != ~0u);
    proxy_casted->bound_index = env_probe->GetRenderResource().GetBufferIndex();

    // temp shit
    env_probe->GetRenderResource().SetTextureSlot(next);

    if (next != ~0u)
    {
        AssertDebug(env_probe->GetPrefilteredEnvMap().IsValid());
        AssertDebug(env_probe->GetPrefilteredEnvMap()->IsReady());

        HYP_LOG(Rendering, Debug, "Setting env probe texture at index: {} to tex with ID: {}", next, env_probe->GetPrefilteredEnvMap().GetID());
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), next, env_probe->GetPrefilteredEnvMap()->GetRenderResource().GetImageView());
        }
    }
    else
    {
        env_probe->GetRenderResource().DecRef();
        env_probe->GetPrefilteredEnvMap()->GetRenderResource().DecRef();
    }
}

void OnEnvGridBindingChanged(EnvGrid* env_grid, uint32 prev, uint32 next)
{
    AssertDebug(env_grid != nullptr);

    IRenderProxy* proxy = RendererAPI_GetRenderProxy(env_grid->GetID());
    AssertThrow(proxy != nullptr);

    RenderProxyEnvGrid* proxy_casted = static_cast<RenderProxyEnvGrid*>(proxy);

    // temp shit
    // proxy_casted->bound_index = next;

    AssertDebug(env_grid->GetRenderResource().GetBufferIndex() != ~0u);
    proxy_casted->bound_index = env_grid->GetRenderResource().GetBufferIndex();

    if (next != ~0u)
    {
        switch (env_grid->GetEnvGridType())
        {
        case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        {
            AssertDebug(env_grid->GetLightFieldIrradianceTexture().IsValid());
            AssertDebug(env_grid->GetLightFieldDepthTexture().IsValid());

            // @TODO: Set based on binding index
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
                    ->SetElement(NAME("LightFieldColorTexture"), env_grid->GetLightFieldIrradianceTexture()->GetRenderResource().GetImageView());

                g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
                    ->SetElement(NAME("LightFieldDepthTexture"), env_grid->GetLightFieldDepthTexture()->GetRenderResource().GetImageView());
            }

            return;
        }
        default:
            break;
        }

        if (env_grid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
        {
            AssertDebug(env_grid->GetVoxelGridTexture().IsValid());

            // Set our voxel grid texture in the global descriptor set so we can use it in shaders
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
                    ->SetElement(NAME("VoxelGridTexture"), env_grid->GetVoxelGridTexture()->GetRenderResource().GetImageView());
            }
        }
    }
}

void OnLightBindingChanged(Light* light, uint32 prev, uint32 next)
{
    AssertDebug(light != nullptr);

    IRenderProxy* proxy = RendererAPI_GetRenderProxy(light->GetID());
    AssertThrow(proxy != nullptr);

    RenderProxyLight* proxy_casted = static_cast<RenderProxyLight*>(proxy);

    // temp shit
    // proxy_casted->bound_index = next;

    proxy_casted->bound_index = light->GetRenderResource().GetBufferIndex();
    AssertDebug(proxy_casted->bound_index != ~0u);
}

} // namespace hyperion