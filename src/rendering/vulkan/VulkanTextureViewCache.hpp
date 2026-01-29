/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/TextureViewCache.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <core/containers/SparsePagedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/SharedMutex.hpp>

namespace Hyperion {

class VulkanTextureViewCache final : public TextureViewCacheBase
{
public:
    SharedMutex mutex;
    // map texture ID -> image views
    SparsePagedArray<HashMap<ImageSubResource, VulkanGpuImageViewRef>, 1024> imageViews;
    // to keep texture IDs as valid
    SparsePagedArray<WeakHandle<Texture>, 1024> weakTextureHandles;

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

    const VulkanGpuImageViewRef& GetOrCreate(Texture* texture, const ImageSubResource& subResource) override;

    void RemoveTexture(const Texture* texture) override;
    void CleanupUnusedTextures() override;
};

} // namespace Hyperion