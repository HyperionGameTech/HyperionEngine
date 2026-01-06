#include <RenderingPch.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/DescriptorSet.hpp>

#include <rendering/renderers/EnvGridRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <rendering/util/ResourceBinder.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/animation/Skeleton.hpp>

namespace Hyperion {

namespace RenderApi {
extern ResourceBinderBase* g_reflectionProbeTextureBinder;
} // namespace RenderApi

void OnBindingChanged_MeshEntity(Entity* entity, uint32 prev, uint32 next)
{
    AssertDebug(entity->InstanceClass() == Entity::StaticClass(),
        "Cannot use Entity subclass as MeshEntity, indices would overlap! Class: {}",
        entity->InstanceClass()->GetName());

    // For now, use Entity ID as index.
    RenderApi::AssignResourceBinding(entity, entity->Id().ToIndex());
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
    proxyCasted->bufferData.materialIndex = RenderApi::RetrieveResourceBinding(proxyCasted->material);
    proxyCasted->bufferData.skeletonIndex = RenderApi::RetrieveResourceBinding(proxyCasted->skeleton);

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

void OnBindingChanged_Mesh(Mesh* mesh, uint32 prev, uint32 next)
{
    AssertDebug(mesh != nullptr);

    if (next != ~0u)
    {
        if ((mesh->GetFlags() & MF_VIEW_INDEPENDENT) || !mesh->gpuUploadFence.IsSignaled())
        {
            mesh->UploadGpuData();
        }
    }
    else if (prev != ~0u)
    {
        if (mesh->gpuUploadFence.IsSignaled() && !(mesh->GetFlags() & MF_VIEW_INDEPENDENT))
        {
            mesh->ReleaseGpuData();
        }
    }
}

void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    Assert(envProbe->IsA<SkyProbe>() || envProbe->IsA<ReflectionProbe>(),
        "EnvProbe must be a SkyProbe or ReflectionProbe, but is a {}", envProbe->InstanceClass()->GetName());

    if (prev != ~0u)
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("EnvProbeTextures"_sh, prev, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->placeholderData->defaultCubemap));
        }
    }

    if (next != ~0u)
    {
        IRenderProxy* proxy = RenderApi::GetRenderProxy(envProbe);
        AssertDebug(proxy != nullptr);

        if (!proxy)
        {
            return;
        }

        RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
        AssertDebug(proxyCasted->envProbe.GetUnsafe() == envProbe);

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("EnvProbeTextures"_sh, next,
                    proxyCasted->texture != nullptr
                        ? g_renderInterface->textureViewCache->GetOrCreate(MakeStrongRef(proxyCasted->texture))
                        : g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->placeholderData->defaultCubemap));
        }
    }
}

void OnBindingChanged_EnvProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    RenderApi::AssignResourceBinding(envProbe, next);
}

void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    if (proxyCasted->envProbe.GetUnsafe()->IsA<SkyProbe>() || proxyCasted->envProbe.GetUnsafe()->IsA<ReflectionProbe>())
    {
        const uint32 textureBinding = RenderApi::g_reflectionProbeTextureBinder->GetBindingForObject(proxyCasted->envProbe.GetUnsafe());
        Assert(textureBinding != ~0u);

        proxyCasted->bufferData.textureIndex = textureBinding;
    }

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next)
{
    AssertDebug(envGrid != nullptr);

    if (!envGrid->IsA<LegacyEnvGrid>())
    {
        return;
    }

    LegacyEnvGrid* legacyEnvGrid = static_cast<LegacyEnvGrid*>(envGrid);

    RenderApi::AssignResourceBinding(envGrid, next);

    switch (legacyEnvGrid->GetEnvGridType())
    {
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
    {
        AssertDebug(legacyEnvGrid->GetLightFieldIrradianceTexture().IsValid());
        AssertDebug(legacyEnvGrid->GetLightFieldDepthTexture().IsValid());

        /// \todo : Set based on binding index
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("LightFieldColorTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(legacyEnvGrid->GetLightFieldIrradianceTexture()));

            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("LightFieldDepthTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(legacyEnvGrid->GetLightFieldDepthTexture()));
        }

        break;
    }
    default:
        break;
    }

    if (legacyEnvGrid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        AssertDebug(legacyEnvGrid->GetVoxelGridTexture().IsValid());

        // Set our voxel grid texture in the global descriptor set so we can use it in shaders
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("VoxelGridTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(legacyEnvGrid->GetVoxelGridTexture()));
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

        const uint32 boundIndex = RenderApi::RetrieveResourceBinding(envProbe);

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

    RenderApi::AssignResourceBinding(light, next);
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
        const uint32 materialBoundIndex = RenderApi::RetrieveResourceBinding(proxyCasted->lightMaterial);
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
    AssertOnThread(g_renderThread);

    static const IRenderConfig& s_renderConfig = g_renderBackend->GetRenderConfig();

    AssertDebug(material != nullptr);

    RenderApi::AssignResourceBinding(material, next);

    //// \todo : Needs to notify that mesh descriptions buffer needs to be updated for ray tracing.

    if (prev != ~0u)
    {
        if (g_renderInterface->materialDescriptorSetManager->imageViews.HasIndex(prev))
        {
            auto& imageViews = g_renderInterface->materialDescriptorSetManager->imageViews.Get(prev);

            if (imageViews.Any())
            {
                SafeDelete(std::move(imageViews));
            }
        }
    }
    
    if (next != ~0u)
    {
        IRenderProxy* proxy = RenderApi::GetRenderProxy(material);
        Assert(proxy != nullptr);

        RenderProxyMaterial* proxyCasted = static_cast<RenderProxyMaterial*>(proxy);

        auto imageViewsIt = g_renderInterface->materialDescriptorSetManager->imageViews.Emplace(next);
        auto& imageViews = *imageViewsIt;

        if (imageViews.Size() < proxyCasted->boundTextures.Size())
        {
            imageViews.Resize(proxyCasted->boundTextures.Size());
        }

        for (uint32 i = 0; i < uint32(proxyCasted->boundTextures.Size()); i++)
        {
            if (imageViews[i].IsValid())
            {
                if (imageViews[i]->GetImage() == proxyCasted->boundTextures[i]->GetGpuImage())
                {
                    continue; // skip; already valid image view set
                }
                
                // defer release until a few frames from now
                SafeDelete(std::move(imageViews[i]));
            }

            imageViews[i] = g_renderBackend->GetTextureImageView(proxyCasted->boundTextures[i]);
        }
    }
}

void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next)
{
    static const IRenderConfig& s_renderConfig = g_renderBackend->GetRenderConfig();
    static const bool s_isBindlessSupported = s_renderConfig.bindlessTextures;

    if (s_isBindlessSupported)
    {
        if (next != ~0u)
        {
            g_renderInterface->bindlessStorage->AddResource(texture->Id(), g_renderInterface->textureViewCache->GetOrCreate(MakeStrongRef(texture)));
        }
        else
        {
            g_renderInterface->bindlessStorage->RemoveResource(texture->Id());
        }
    }

    RenderApi::AssignResourceBinding(texture, next);
}

} // namespace Hyperion
