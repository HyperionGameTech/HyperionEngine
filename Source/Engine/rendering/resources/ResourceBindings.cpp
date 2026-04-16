#include <RenderingPch.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/DescriptorSet.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <engine/resources/ResourceBinder.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/animation/Skeleton.hpp>

namespace Hyperion {

namespace Resources {

extern ResourceBinderBase* g_reflectionProbeTextureBinder;

void WriteBufferData_MeshEntity(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(idx != ~0u);

    RenderProxyMesh* proxyCasted = static_cast<RenderProxyMesh*>(proxy);

    proxyCasted->bufferData.entityIndex = idx;
    proxyCasted->bufferData.materialIndex = GetBinding(proxyCasted->material);
    proxyCasted->bufferData.skeletonIndex = GetBinding(proxyCasted->skeleton);
    
    sbuffer.Write(idx * sizeof(proxyCasted->bufferData), sizeof(proxyCasted->bufferData), &proxyCasted->bufferData);
}

void OnBindingChanged_Mesh(Mesh* mesh, uint32 prev, uint32 next)
{
    AssertDebug(mesh != nullptr);

    if (next != ~0u)
    {
        if ((mesh->GetFlags() & MeshFlags::ViewIndependent) || !mesh->gpuUploadSemaphore.IsSignaled())
        {
            mesh->UploadGpuData();
        }
    }
    else if (prev != ~0u)
    {
        if (mesh->gpuUploadSemaphore.IsSignaled() && !(mesh->GetFlags() & MeshFlags::ViewIndependent))
        {
            mesh->ReleaseGpuData();
        }
    }

    SetBinding(mesh, next);
}

void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
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

        CommandRecorder& cr = currentFrame->preRenderCommands;

        cr << InsertBarrier(srcImage, RS_COPY_SRC);
        cr << InsertBarrier(dstImage, RS_COPY_DST);

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
            dstSubResource.baseArrayLayer = uint16(6 * next);
            dstSubResource.numLayers = 6;

            const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(mipIndex);
            const Vec3u dstMipExtent = dstImage->GetTextureDesc().GetMipExtent(mipIndex);

            cr << Blit(
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

        cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
        cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }
}

void WriteBufferData_EnvProbe(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(idx != ~0u);

    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    if (proxyCasted->envProbe.GetUnsafe()->IsA<SkyProbe>() || proxyCasted->envProbe.GetUnsafe()->IsA<ReflectionProbe>())
    {
        const uint32 textureBinding = Resources::g_reflectionProbeTextureBinder->GetBindingForObject(proxyCasted->envProbe.GetUnsafe());
        Assert(textureBinding != ~0u);

        proxyCasted->bufferData.textureIndex = textureBinding;
    }

    sbuffer.Write(idx * sizeof(proxyCasted->bufferData), sizeof(proxyCasted->bufferData), &proxyCasted->bufferData);
}

void WriteBufferData_Light(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(idx != ~0u);

    RenderProxyLight* proxyCasted = static_cast<RenderProxyLight*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    LightShaderData& bufferData = proxyCasted->bufferData;

    // textured area lights can have a material attached
    if (proxyCasted->lightMaterial != nullptr)
    {
        const uint32 materialBoundIndex = GetBinding(proxyCasted->lightMaterial);
        AssertDebug(materialBoundIndex != ~0u, "Light uses Material {} but it is not bound", proxyCasted->lightMaterial->Id());

        bufferData.materialIndex = materialBoundIndex;
    }
    else
    {
        bufferData.materialIndex = ~0u;
    }
    
    sbuffer.Write(idx * sizeof(bufferData), sizeof(bufferData), &bufferData);
}

void OnBindingChanged_Material(MaterialInstance* material, uint32 prev, uint32 next)
{
    AssertOnThread(g_renderThread);

    static const IRenderConfig& s_renderConfig = g_renderInterface->GetRenderConfig();

    AssertDebug(material != nullptr);

    SetBinding(material, next);

    if (prev != ~0u)
    {
        if (g_renderInterface->materialTextureCache->imageViews.HasIndex(prev))
        {
            auto& imageViews = g_renderInterface->materialTextureCache->imageViews.Get(prev);

            if (imageViews.Any())
            {
                EnqueueDeletion(std::move(imageViews));
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
                EnqueueDeletion(std::move(imageViews[i]));
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

    SetBinding(texture, next);
}

} // namespace Resources
} // namespace Hyperion
