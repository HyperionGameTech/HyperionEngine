#include <RenderingPch.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/DescriptorSet.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <rendering/util/ResourceBinder.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/animation/Skeleton.hpp>

namespace Hyperion {

extern ResourceBinderBase* g_reflectionProbeTextureBinder;

void OnBindingChanged_MeshEntity(Entity* entity, uint32 prev, uint32 next)
{
    AssertDebug(entity->InstanceClass() == Entity::StaticClass(),
        "Cannot use Entity subclass as MeshEntity, indices would overlap! Class: {}",
        entity->InstanceClass()->GetName());

    // For now, use Entity ID as index.
    AssignResourceBinding(entity, entity->Id().ToIndex());
}

HYP_DISABLE_OPTIMIZATION;
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
    proxyCasted->bufferData.materialIndex = RetrieveResourceBinding(proxyCasted->material);
    proxyCasted->bufferData.skeletonIndex = RetrieveResourceBinding(proxyCasted->skeleton);

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}
HYP_ENABLE_OPTIMIZATION;

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

    if (next != ~0u)
    {
        IRenderProxy* proxy = GetRenderProxy(envProbe);
        AssertDebug(proxy != nullptr);

        if (!proxy)
        {
            return;
        }

        RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
        AssertDebug(proxyCasted->envProbe.GetUnsafe() == envProbe);

        if (!proxyCasted->texture)
        {
            HYP_LOG(Rendering, Warning, "No EnvProbe texture for {}", envProbe->Id());

            return;
        }

        // blit to the array texture
        const GpuImageRef& srcImage = proxyCasted->texture->GetGpuImage();
        AssertDebug(srcImage.IsValid());

        const GpuImageRef& dstImage = g_renderInterface->envProbesTexture->GetGpuImage();
        Assert(dstImage.IsValid());

        Frame* currentFrame = g_renderInterface->GetCurrentFrame();
        Assert(currentFrame != nullptr);

        RenderQueue& rq = currentFrame->preRenderQueue;

        rq << InsertBarrier(srcImage, RS_COPY_SRC);
        rq << InsertBarrier(dstImage, RS_COPY_DST);

        for (uint8 mipIndex = 0; mipIndex < dstImage->NumMips(); mipIndex++)
        {
            if (mipIndex >= srcImage->NumMips())
            {
                break;
            }
            
            ImageSubResource srcSubResource {};
            srcSubResource.baseMipLevel = mipIndex;
            srcSubResource.baseArrayLayer = 0;
            srcSubResource.numLayers = 6;

            ImageSubResource dstSubResource {};
            dstSubResource.baseMipLevel = mipIndex;
            dstSubResource.baseArrayLayer = 6 * next;
            dstSubResource.numLayers = 6;

            const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(mipIndex);
            const Vec3u dstMipExtent = dstImage->GetTextureDesc().GetMipExtent(mipIndex);

            rq << Blit(
                srcImage,
                dstImage,
                Rect<uint32> {
                    0, 0,
                    srcMipExtent.x, srcMipExtent.y
                },
                Rect<uint32> {
                    0, 0,
                    dstMipExtent.x, dstMipExtent.y
                },
                srcSubResource,
                dstSubResource);
        }

        rq << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
        rq << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }
}

void OnBindingChanged_EnvProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    AssertDebug(envProbe != nullptr);
    AssertDebug(envProbe->IsReady());

    AssignResourceBinding(envProbe, next);
}

void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u);

    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    if (proxyCasted->envProbe.GetUnsafe()->IsA<SkyProbe>() || proxyCasted->envProbe.GetUnsafe()->IsA<ReflectionProbe>())
    {
        const uint32 textureBinding = g_reflectionProbeTextureBinder->GetBindingForObject(proxyCasted->envProbe.GetUnsafe());
        Assert(textureBinding != ~0u);

        proxyCasted->bufferData.textureIndex = textureBinding;
    }

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next)
{
    AssertDebug(envGrid != nullptr);

    AssignResourceBinding(envGrid, next);
}

void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next)
{
    AssertDebug(light != nullptr);

    AssignResourceBinding(light, next);
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
        const uint32 materialBoundIndex = RetrieveResourceBinding(proxyCasted->lightMaterial);
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

    static const IRenderConfig& s_renderConfig = g_renderInterface->GetRenderConfig();

    AssertDebug(material != nullptr);

    AssignResourceBinding(material, next);

    //// \todo : Needs to notify that mesh descriptions buffer needs to be updated for ray tracing.

    if (prev != ~0u)
    {
        if (g_renderInterface->materialTextureCache->imageViews.HasIndex(prev))
        {
            auto& imageViews = g_renderInterface->materialTextureCache->imageViews.Get(prev);

            if (imageViews.Any())
            {
                SafeDelete(std::move(imageViews));
            }
        }
    }
    
    if (next != ~0u)
    {
        IRenderProxy* proxy = GetRenderProxy(material);
        Assert(proxy != nullptr);

        RenderProxyMaterial* proxyCasted = static_cast<RenderProxyMaterial*>(proxy);

        auto imageViewsIt = g_renderInterface->materialTextureCache->imageViews.Emplace(next);
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

            imageViews[i] = g_renderInterface->textureViewCache->GetOrCreate(proxyCasted->boundTextures[i]);
        }
    }
}

void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next)
{
    static const IRenderConfig& s_renderConfig = g_renderInterface->GetRenderConfig();
    static const bool s_isBindlessSupported = s_renderConfig.bindlessTextures;

    if (s_isBindlessSupported)
    {
        if (next != ~0u)
        {
            g_renderInterface->bindlessStorage->AddResource(BindlessStorage_Textures, texture->Id().ToIndex(), g_renderInterface->textureViewCache->GetOrCreate(texture));
        }
        else
        {
            g_renderInterface->bindlessStorage->RemoveResource(BindlessStorage_Textures, texture->Id().ToIndex());
        }
    }

    AssignResourceBinding(texture, next);
}

} // namespace Hyperion
