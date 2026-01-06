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
            SafeDelete(std::move(jt));
        }
    }
}

const VulkanGpuImageViewRef& VulkanTextureViewCache::GetOrCreate(const Handle<Texture>& texture, const ImageSubResource& subResource)
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

    HYP_GFX_ASSERT(texture.IsValid());

    const SizeType idx = texture.Id().ToIndex();

    if (!imageViews.HasIndex(idx))
    {
        imageViews.Emplace(idx);
        weakTextureHandles.Emplace(idx, texture.ToWeak());
    }

    auto& textureImageViews = imageViews.Get(idx);

    auto it = textureImageViews.Find(subResource);

    if (it == textureImageViews.End())
    {

        VulkanGpuImageViewRef imageView = CreateObject<VulkanGpuImageView>(
            VulkanGpuImageRef(texture->GetGpuImage()),
            subResource.baseMipLevel,
            subResource.numLevels,
            subResource.baseArrayLayer,
            subResource.numLayers);

        HYP_GFX_ASSERT(imageView->Create());

        it = textureImageViews.Set(subResource, imageView).first;
    }

    HYP_GFX_ASSERT(it->second.IsValid());

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

            HYP_GFX_ASSERT(imageViews.HasIndex(idx));
            HYP_GFX_ASSERT(weakTextureHandles.HasIndex(idx));

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
