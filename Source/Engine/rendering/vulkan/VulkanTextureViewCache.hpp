/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/TextureViewCache.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/containers/SparsePagedArray.hpp>
#include <Core/containers/HashMap.hpp>

#include <Core/threading/SharedMutex.hpp>

namespace Hyperion {

class VulkanTextureViewCache final : public TextureViewCacheBase
{
public:
    using TextureImageViewMap = HashMap<uint64, VulkanGpuImageViewRef, PooledNodeAllocator<VulkanAllocator>>;

    SharedMutex mutex;
    // map texture ID -> image views
    SparsePagedArray<TextureImageViewMap, 32, VulkanAllocator> imageViews;
    // to keep texture IDs as valid
    SparsePagedArray<WeakHandle<Texture>, 32, VulkanAllocator> weakTextureHandles;

    typename decltype(weakTextureHandles)::Iterator cleanupIterator;

    VulkanTextureViewCache()
    {
        cleanupIterator = weakTextureHandles.End();
    }

    ~VulkanTextureViewCache() override;

    const VulkanGpuImageViewRef& GetOrCreate(
        Texture* texture,
        uint32 mipIndex = 0,
        uint32 numMips = ~0u,
        uint32 layerIndex = 0,
        uint32 numLayers = ~0u) override;

    const VulkanGpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource) override;

    const GpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource,
        TextureType viewTextureType) override;

    void RemoveTexture(const Texture* texture) override;
    void CleanupUnusedTextures() override;
};

} // namespace Hyperion