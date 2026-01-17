/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanTextureViewCache.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>

#include <rendering/Texture.hpp>

#include <rendering/util/SafeDeleter.hpp>

namespace Hyperion {

extern VulkanRenderBackend* g_renderBackend;

VulkanTextureViewCache::~VulkanTextureViewCache()
{
    for (auto& it : imageViews)
    {
        for (auto& jt : it)
        {
            SafeDelete(std::move(jt.second));
        }
    }
}

const VulkanGpuImageViewRef& VulkanTextureViewCache::GetOrCreate(
    const Handle<Texture>& texture,
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

const VulkanGpuImageViewRef& VulkanTextureViewCache::GetOrCreate(const Handle<Texture>& texture, const ImageSubResource& subResource)
{
    AssertOnThread(g_renderThread);

    Assert(texture.IsValid());

    const SizeType idx = texture.Id().ToIndex();

    TSharedLock sharedLock(mutex);

    if (!imageViews.HasIndex(idx))
    {
        imageViews.Emplace(idx);
        weakTextureHandles.Emplace(idx, texture.ToWeak());
    }

    auto& textureImageViews = imageViews.Get(idx);

    ValueStorage<TUniqueLock<SharedMutex>> uniqueLockStorage {};
    bool isLockUnique = false;
    HYP_DEFER({ if (isLockUnique) uniqueLockStorage.Destruct(); });

    auto it = textureImageViews.Find(subResource);

    if (it == textureImageViews.End())
    {
        VulkanGpuImageViewRef imageView = CreateObject<VulkanGpuImageView>(
            texture->GetGpuImage(),
            subResource.baseMipLevel,
            subResource.numLevels,
            subResource.baseArrayLayer,
            subResource.numLayers);

        Assert(imageView->Create());

        sharedLock.Reset();

        uniqueLockStorage.Construct(mutex);

        isLockUnique = true;

        it = textureImageViews.Set(subResource, imageView).first;
    }

    Assert(it->second.IsValid());

    return it->second;
}

void VulkanTextureViewCache::RemoveTexture(const Texture* texture)
{
    AssertOnThread(g_renderThread);

    if (!texture)
    {
        return;
    }

    const SizeType idx = texture->Id().ToIndex();

    TUniqueLock lock(mutex);

    if (imageViews.HasIndex(idx))
    {
        for (auto& it : imageViews.Get(idx))
        {
            SafeDelete(std::move(it.second));
        }

        imageViews.EraseAt(idx);
        weakTextureHandles.EraseAt(idx);
    }
}

void VulkanTextureViewCache::CleanupUnusedTextures()
{
    AssertOnThread(g_renderThread);

    TUniqueLock lock(mutex);

    constexpr uint32 MaxCycles = 32;

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

    for (uint32 i = 0; cleanupIterator != weakTextureHandles.End() && i < MaxCycles; i++)
    {
        auto& entry = *cleanupIterator;

        if (!entry.Lock())
        {
            const SizeType idx = weakTextureHandles.IndexOf(cleanupIterator);

            Assert(imageViews.HasIndex(idx));
            Assert(weakTextureHandles.HasIndex(idx));

            for (auto& it : imageViews.Get(idx))
            {
                SafeDelete(std::move(it.second));
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
        HYP_LOG(RenderingBackend, Debug, "VulkanTextureCache: Cleaned up {} unused textures", numRemoved);
    }
}

} // namespace Hyperion
