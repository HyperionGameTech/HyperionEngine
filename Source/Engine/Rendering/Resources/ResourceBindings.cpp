#include <RenderingPch.hpp>

#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/MaterialTextureCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Bindless.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/DescriptorSet.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>

#include <Framework/Resources/ResourceBinder.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/Light.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Animation/Skeleton.hpp>

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
        if (!mesh->isUploaded.Load())
        {
            mesh->UploadGpuData();
        }
    }
    else if (prev != ~0u)
    {
        if (!(mesh->GetFlags() & MeshFlags::ViewIndependent))
        {
            mesh->ReleaseGpuData();
        }
    }

    SetBinding(mesh, next);
}

void OnBindingChanged_EnvProbe(EnvProbe* envProbe, uint32 prev, uint32 next)
{
    SetBinding(envProbe, next);

    if (next != ~0u)
    {
        IRenderProxy* proxy = GetRenderProxy(envProbe);
        AssertDebug(proxy != nullptr);

        if (!proxy)
        {
            return;
        }

        RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
        AssertDebug(proxyCasted->envProbe == envProbe);

        // depth
        if (proxyCasted->visibilityTexture != nullptr)
        {
            CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

            Texture* srcTexture = proxyCasted->visibilityTexture;
            Texture* dstTexture = RI.envProbesDepthTexture;

            // blit to the array texture
            GpuImage* srcImage = srcTexture->GetGpuImage();
            AssertDebug(srcImage != nullptr);

            GpuImage* dstImage = dstTexture->GetGpuImage();
            AssertDebug(dstImage != nullptr);

            ImageSubResource srcSubResource {};
            srcSubResource.baseMipLevel = 0;
            srcSubResource.numLevels = 1;
            srcSubResource.baseArrayLayer = 0;
            srcSubResource.numLayers = 6;

            ImageSubResource dstSubResource {};
            dstSubResource.baseMipLevel = 0;
            dstSubResource.numLevels = 1;
            dstSubResource.baseArrayLayer = uint16(6 * next);
            dstSubResource.numLayers = 6;

            cr << InsertBarrier(srcImage, RS_COPY_SRC, srcSubResource);
            cr << InsertBarrier(dstImage, RS_COPY_DST, dstSubResource);

            const Vec3u srcExtent = srcImage->GetTextureDesc().extent;
            const Vec3u dstExtent = dstImage->GetTextureDesc().extent;

            AssertDebug(srcExtent == dstExtent && srcImage->GetTextureDesc().format == dstImage->GetTextureDesc().format);

            cr << CopyImage(srcImage, dstImage, srcExtent, srcSubResource, dstSubResource);

            cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE, srcSubResource);
            cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE, dstSubResource);

            cr.Done();
        }
    }
}

void WriteBufferData_EnvProbe(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(idx != ~0u);

    RenderProxyEnvProbe* proxyCasted = static_cast<RenderProxyEnvProbe*>(proxy);
    AssertDebug(proxyCasted != nullptr);

    proxyCasted->bufferData.textureIndex = 0;

    if (proxyCasted->envProbe->IsA<SkyProbe>()
        || proxyCasted->envProbe->IsA<ReflectionProbe>())
    {
        const uint32 colorTextureBinding = Resources::g_reflectionProbeTextureBinder->GetBindingForObject(proxyCasted->envProbe);
        AssertDebug(colorTextureBinding != ~0u);
        AssertDebug(colorTextureBinding < 0xFFFFu); // we consider anything >= 0xFFFFu to be invalid

        proxyCasted->bufferData.textureIndex |= (colorTextureBinding & 0xFFFFu);
    }

    // If it has a visibility texture, it will be bound using env probe binding slot,
    // as irradiance probes can also use visibility texture
    if (proxyCasted->visibilityTexture != nullptr)
    {
        AssertDebug(idx < 0xFFFFu);

        proxyCasted->bufferData.textureIndex |= ((idx & 0xFFFFu) << 16);
    }

    sbuffer.Write(idx * sizeof(proxyCasted->bufferData), sizeof(proxyCasted->bufferData), &proxyCasted->bufferData);
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
        AssertDebug(proxyCasted->envProbe == envProbe);

        if (!proxyCasted->texture)
        {
            HYP_LOG(Rendering, Warning, "No EnvProbe texture for {}", envProbe->Id());

            return;
        }

        CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

        Texture* srcTexture = proxyCasted->texture;
        Texture* dstTexture = RI.envProbesColorTexture;

        // blit to the array texture
        GpuImage* srcImage = srcTexture->GetGpuImage();
        AssertDebug(srcImage != nullptr);

        GpuImage* dstImage = dstTexture->GetGpuImage();
        AssertDebug(dstImage != nullptr);

        // Scope barriers to just this probe's 6-layer slice of the shared array texture -
        // a whole-image barrier would transition every other probe's layers too, racing
        // against their in-flight sampling of this same array (e.g. during concurrent bakes).
        ImageSubResource srcAllMipsSubResource {};
        srcAllMipsSubResource.baseMipLevel = 0;
        srcAllMipsSubResource.numLevels = srcImage->NumMips();
        srcAllMipsSubResource.baseArrayLayer = 0;
        srcAllMipsSubResource.numLayers = 6;

        ImageSubResource dstAllMipsSubResource {};
        dstAllMipsSubResource.baseMipLevel = 0;
        dstAllMipsSubResource.numLevels = dstImage->NumMips();
        dstAllMipsSubResource.baseArrayLayer = uint16(6 * next);
        dstAllMipsSubResource.numLayers = 6;

        cr << InsertBarrier(srcImage, RS_COPY_SRC, srcAllMipsSubResource);
        cr << InsertBarrier(dstImage, RS_COPY_DST, dstAllMipsSubResource);

        for (uint8 mipIndex = 0; mipIndex < dstImage->NumMips(); mipIndex++)
        {
            if (mipIndex >= srcImage->NumMips())
            {
                break;
            }

            ImageSubResource srcSubResource {};
            srcSubResource.baseMipLevel = mipIndex;
            srcSubResource.numLevels = 1;
            srcSubResource.baseArrayLayer = 0;
            srcSubResource.numLayers = 6;

            ImageSubResource dstSubResource {};
            dstSubResource.baseMipLevel = mipIndex;
            dstSubResource.numLevels = 1;
            dstSubResource.baseArrayLayer = uint16(6 * next);
            dstSubResource.numLayers = 6;

            const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(mipIndex);
            const Vec3u dstMipExtent = dstImage->GetTextureDesc().GetMipExtent(mipIndex);

            if (srcMipExtent == dstMipExtent && srcImage->GetTextureDesc().format == dstImage->GetTextureDesc().format)
            {
                cr << CopyImage(srcImage, dstImage, srcMipExtent, srcSubResource, dstSubResource);
            }
            else
            {
                cr << Blit(
                    srcTexture,
                    dstTexture,
                    Rect<uint32> { 0, 0, srcMipExtent.x, srcMipExtent.y },
                    Rect<uint32> { 0, 0, dstMipExtent.x, dstMipExtent.y },
                    srcSubResource,
                    dstSubResource);
            }
        }

        cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE, srcAllMipsSubResource);
        cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE, dstAllMipsSubResource);

        cr.Done();
    }
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

void OnBindingChanged_Material(Material* material, uint32 prev, uint32 next)
{
    AssertOnThread(g_renderThread);

    static const IRenderConfig& s_renderConfig = RI.GetRenderConfig();

    AssertDebug(material != nullptr);

    SetBinding(material, next);

    if (prev != ~0u)
    {
        if (RI.materialTextureCache->imageViews.HasIndex(prev))
        {
            auto& imageViews = RI.materialTextureCache->imageViews.Get(prev);

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

        auto& imageViews = *RI.materialTextureCache->imageViews.Emplace(next);

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

            imageViews[i] = RI.textureViewCache->GetOrCreate(proxyCasted->boundTextures[i]);
        }
    }
}

void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next)
{
    static const IRenderConfig& s_renderConfig = RI.GetRenderConfig();
    static const bool s_isBindlessSupported = s_renderConfig.bindlessTextures;

    if (s_isBindlessSupported)
    {
        if (next != ~0u)
        {
            // @TODO Use 'next' rather than texture->Id().ToIndex() here
            RI.bindlessStorage->AddResource(BindlessStorage_Textures, texture->Id().ToIndex(), RI.textureViewCache->GetOrCreate(texture));
        }
        else
        {
            // @TODO Use 'prev' rather than texture->Id().ToIndex() here
            RI.bindlessStorage->RemoveResource(BindlessStorage_Textures, texture->Id().ToIndex());
        }
    }

    SetBinding(texture, next);
}

} // namespace Resources
} // namespace Hyperion
