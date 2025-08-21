#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>
#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Bindless.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Material.hpp>
#include <scene/animation/Skeleton.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Defines.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

void OnBindingChanged_MeshEntity(Entity* entity, uint32 prev, uint32 next)
{
    AssertDebug(entity->InstanceClass() == Entity::Class(),
        "Cannot use Entity subclass as MeshEntity, indices would overlap! Class: {}",
        entity->InstanceClass()->GetName());

    // For now, use Entity ID as index.
    RenderApi_AssignResourceBinding(entity, entity->Id().ToIndex());
}

void WriteBufferData_MeshEntity(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    RenderProxyMesh* proxyCasted = static_cast<RenderProxyMesh*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    AssertDebug(idx == proxyCasted->entity.Id().ToIndex());
    AssertDebug(proxyCasted->entity.Id().GetTypeId() == TypeId::ForType<Entity>(),
        "Cannot use Entity subclass as MeshEntity, indices would overlap! Class: {}",
        LookupTypeName(proxyCasted->entity.Id().GetTypeId()));

    proxyCasted->bufferData.entityIndex = proxyCasted->entity.Id().ToIndex();
    proxyCasted->bufferData.materialIndex = RenderApi_RetrieveResourceBinding(proxyCasted->material);
    proxyCasted->bufferData.skeletonIndex = RenderApi_RetrieveResourceBinding(proxyCasted->skeleton);

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    Assert(envProbe->IsA<SkyProbe>() || envProbe->IsA<ReflectionProbe>(),
        "EnvProbe must be a SkyProbe or ReflectionProbe, but is: %s", envProbe->InstanceClass()->GetName());

    if (!envProbe->GetPrefilteredEnvMap().IsValid())
    {
        HYP_LOG(Rendering, Error, "EnvProbe {} (class: {}) has no prefiltered env map set!\n", envProbe->Id(),
            envProbe->InstanceClass()->GetName());

        return;
    }

    if (prev != ~0u)
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("EnvProbeTextures", prev, g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
        }
    }

    RenderApi_AssignResourceBinding(envProbe, next);

    if (next != ~0u)
    {
        AssertDebug(envProbe->GetPrefilteredEnvMap().IsValid());
        AssertDebug(envProbe->GetPrefilteredEnvMap()->IsReady());

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("EnvProbeTextures", next, g_renderBackend->GetTextureImageView(envProbe->GetPrefilteredEnvMap()));
        }
    }
}

void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    if (proxyCasted->envProbe.GetUnsafe()->IsA<SkyProbe>() || proxyCasted->envProbe.GetUnsafe()->IsA<ReflectionProbe>())
    {
        proxyCasted->bufferData.textureIndex = idx;
    }
    else
    {
        proxyCasted->bufferData.textureIndex = ~0u;
    }

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

void OnBindingChanged_AmbientProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    AssertDebug(envProbe->GetEnvProbeType() == EPT_AMBIENT);

    RenderApi_AssignResourceBinding(envProbe, next);
}

void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next)
{
    AssertDebug(envGrid != nullptr);

    if (!envGrid->IsA<LegacyEnvGrid>())
    {
        return;
    }

    LegacyEnvGrid* legacyEnvGrid = static_cast<LegacyEnvGrid*>(envGrid);

    RenderApi_AssignResourceBinding(envGrid, next);

    switch (legacyEnvGrid->GetEnvGridType())
    {
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
    {
        AssertDebug(legacyEnvGrid->GetLightFieldIrradianceTexture().IsValid());
        AssertDebug(legacyEnvGrid->GetLightFieldDepthTexture().IsValid());

        // @TODO: Set based on binding index
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("LightFieldColorTexture", g_renderBackend->GetTextureImageView(legacyEnvGrid->GetLightFieldIrradianceTexture()));

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("LightFieldDepthTexture", g_renderBackend->GetTextureImageView(legacyEnvGrid->GetLightFieldDepthTexture()));
        }

        return;
    }
    default:
        break;
    }

    if (legacyEnvGrid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        AssertDebug(legacyEnvGrid->GetVoxelGridTexture().IsValid());

        // Set our voxel grid texture in the global descriptor set so we can use it in shaders
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("VoxelGridTexture", g_renderBackend->GetTextureImageView(legacyEnvGrid->GetVoxelGridTexture()));
        }
    }
}

void WriteBufferData_EnvGrid(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    RenderProxyEnvGrid* proxyCasted = static_cast<RenderProxyEnvGrid*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    EnvGrid* envGrid = proxyCasted->envGrid.GetUnsafe();
    AssertDebug(envGrid != nullptr);

    uint32 offset = 0;

    for (auto it = std::begin(proxyCasted->envProbes); it != std::end(proxyCasted->envProbes); ++it)
    {
        EnvProbe* envProbe = *it;

        // at first non-valid id, just set all remaining probe indices to -1
        if (!envProbe)
        {
            std::fill(proxyCasted->bufferData.probeIndices + offset, std::end(proxyCasted->bufferData.probeIndices), ~0u);

            break;
        }

        const uint32 boundIndex = RenderApi_RetrieveResourceBinding(envProbe);

        if (boundIndex == ~0u)
        {
            HYP_LOG(Rendering, Warning, "EnvProbe {} not currently bound when writing buffer data for EnvGrid {}", envProbe->Id(), envGrid->Id());

            continue;
        }

        proxyCasted->bufferData.probeIndices[offset++] = boundIndex;
    }

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next)
{
    AssertDebug(light != nullptr);

    RenderApi_AssignResourceBinding(light, next);
}

void WriteBufferData_Light(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    RenderProxyLight* proxyCasted = static_cast<RenderProxyLight*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    LightShaderData& bufferData = proxyCasted->bufferData;

    // textured area lights can have a material attached
    if (proxyCasted->lightMaterial != nullptr)
    {
        const uint32 materialBoundIndex = RenderApi_RetrieveResourceBinding(proxyCasted->lightMaterial);
        AssertDebug(materialBoundIndex != ~0u, "Light uses Material {} but it is not bound", proxyCasted->lightMaterial->Id());

        bufferData.materialIndex = materialBoundIndex;
    }
    else
    {
        bufferData.materialIndex = ~0u;
    }

    gpuBufferHolder->WriteBufferData(idx, &bufferData, sizeof(bufferData));
}

void OnBindingChanged_Material(Material* material, uint32 prev, uint32 next)
{
    Threads::AssertOnThread(g_renderThread);

    static const IRenderConfig& renderConfig = g_renderBackend->GetRenderConfig();
    static const bool isBindlessSupported = renderConfig.bindlessTextures;

    AssertDebug(material != nullptr);

    RenderApi_AssignResourceBinding(material, next);

    /// @TODO: Needs to notify that mesh descriptions buffer needs to be updated for ray tracing.

    if (!isBindlessSupported)
    {
        if (prev != ~0u)
        {
            g_renderGlobalState->materialDescriptorSetManager->Remove(prev);
        }

        if (next != ~0u)
        {
            IRenderProxy* proxy = RenderApi_GetRenderProxy(material);
            Assert(proxy != nullptr);

            RenderProxyMaterial* proxyCasted = static_cast<RenderProxyMaterial*>(proxy);

            g_renderGlobalState->materialDescriptorSetManager->Allocate(
                next,
                proxyCasted->boundTextureIndices.ToSpan(),
                proxyCasted->boundTextures.ToSpan());
        }
    }
}

void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next)
{
    static const IRenderConfig& renderConfig = g_renderBackend->GetRenderConfig();
    static const bool isBindlessSupported = renderConfig.bindlessTextures;

    if (isBindlessSupported)
    {
        if (next != ~0u)
        {
            g_renderGlobalState->bindlessStorage->AddResource(texture->Id(), g_renderBackend->GetTextureImageView(MakeStrongRef(texture)));
        }
        else
        {
            g_renderGlobalState->bindlessStorage->RemoveResource(texture->Id());
        }
    }

    RenderApi_AssignResourceBinding(texture, next);
}

} // namespace hyperion
