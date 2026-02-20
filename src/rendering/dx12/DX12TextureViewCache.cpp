/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12TextureViewCache.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>

#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

DX12TextureViewCache::~DX12TextureViewCache()
{
    for (auto& it : imageViews)
    {
        for (auto& jt : it)
        {
            EnqueueDeletion(std::move(jt.second));
        }
    }
}

const DX12GpuImageViewRef& DX12TextureViewCache::GetOrCreate(
    Texture* texture,
    uint32 mipIndex,
    uint32 numMips,
    uint32 layerIndex,
    uint32 numLayers)
{
    const uint32 maxMipLevel = texture->GetTextureDesc().NumMips() - 1;
    const uint32 maxArrayLayer = texture->GetTextureDesc().NumArrayLayers() - 1;

    ImageSubResource subResource {};
    subResource.numLevels = MathUtil::Min(numMips, maxMipLevel + 1);
    subResource.baseMipLevel = MathUtil::Min(mipIndex, maxMipLevel);
    subResource.numLayers = MathUtil::Min(numLayers, maxArrayLayer + 1);
    subResource.baseArrayLayer = MathUtil::Min(layerIndex, maxArrayLayer);

    return GetOrCreate(texture, subResource);
}

const DX12GpuImageViewRef& DX12TextureViewCache::GetOrCreate(
    Texture* texture, const ImageSubResource& subResource)
{
    Assert(texture != nullptr);

    DX12GpuImage* gpuImage = texture->GetGpuImage();
    AssertDebug(gpuImage != nullptr);

    const TextureDesc& textureDesc = gpuImage->GetTextureDesc();

    // Create view to match texture type
    return GetOrCreate(texture, subResource, textureDesc.type);
}

const DX12GpuImageViewRef& DX12TextureViewCache::GetOrCreate(
    Texture* texture,
    const ImageSubResource& subResource,
    TextureType textureType)
{
    AssertOnThread(g_renderThread);

    Assert(texture != nullptr);

    const SizeType idx = texture->Id().ToIndex();

    if (!imageViews.HasIndex(idx))
    {
        imageViews.Emplace(idx);
        weakTextureHandles.Emplace(idx, MakeWeakRef(texture));
    }

    auto& textureImageViews = imageViews.Get(idx);

    auto it = textureImageViews.Find(subResource);

    if (it == textureImageViews.End())
    {
        DX12GpuImageViewRef imageView = MakeHandle<DX12GpuImageView>(
            texture->GetGpuImage(),
            subResource);

        CheckResult(imageView->Create());

        it = textureImageViews.Set(subResource, imageView).first;
    }

    Assert(it->second.IsValid());

    return it->second;
}

void DX12TextureViewCache::RemoveTexture(const Texture* texture)
{
    AssertOnThread(g_renderThread);

    if (!texture)
    {
        return;
    }

    const SizeType idx = texture->Id().ToIndex();

    if (imageViews.HasIndex(idx))
    {
        for (auto& it : imageViews.Get(idx))
        {
            EnqueueDeletion(std::move(it.second));
        }

        imageViews.EraseAt(idx);
        weakTextureHandles.EraseAt(idx);
    }
}

void DX12TextureViewCache::CleanupUnusedTextures()
{
    AssertOnThread(g_renderThread);

    constexpr uint32 maxCycles = 32;

    cleanupIterator = typename decltype(weakTextureHandles)::Iterator {
        &weakTextureHandles,
        cleanupIterator.page,
        cleanupIterator.elem
    };

    if (cleanupIterator == weakTextureHandles.End())
    {
        cleanupIterator = weakTextureHandles.Begin();
    }

    uint32 numRemoved = 0;

    for (uint32 i = 0; cleanupIterator != weakTextureHandles.End() && i < maxCycles; i++)
    {
        auto& entry = *cleanupIterator;

        if (!entry.Lock())
        {
            const SizeType idx = weakTextureHandles.IndexOf(cleanupIterator);

            Assert(imageViews.HasIndex(idx));
            Assert(weakTextureHandles.HasIndex(idx));

            for (auto& it : imageViews.Get(idx))
            {
                EnqueueDeletion(std::move(it.second));
            }

            imageViews.EraseAt(idx);

            cleanupIterator = weakTextureHandles.Erase(cleanupIterator);

            ++numRemoved;

            continue;
        }

        ++cleanupIterator;
    }

    if (numRemoved != 0)
    {
        HYP_LOG(RenderingBackend, Debug, "DX12TextureViewCache: Cleaned up {} unused textures", numRemoved);
    }
}

} // namespace Hyperion
