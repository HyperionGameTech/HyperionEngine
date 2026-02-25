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

namespace Hyperion {

class DX12TextureViewCache final : public TextureViewCacheBase
{
public:
    // map texture ID -> image views
    SparsePagedArray<HashMap<ImageSubResource, DX12GpuImageViewRef>, 1024> imageViews;
    // to keep texture IDs as valid
    SparsePagedArray<WeakHandle<Texture>, 1024> weakTextureHandles;

    typename decltype(weakTextureHandles)::Iterator cleanupIterator;

    DX12TextureViewCache()
    {
        cleanupIterator = weakTextureHandles.End();
    }

    ~DX12TextureViewCache() override;

    const DX12GpuImageViewRef& GetOrCreate(
        Texture* texture,
        uint32 mipIndex = 0,
        uint32 numMips = ~0u,
        uint32 layerIndex = 0,
        uint32 numLayers = ~0u) override;

    const DX12GpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource) override;

    const DX12GpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource,
        TextureType viewTextureType) override;
    
    void RemoveTexture(const Texture* texture) override;
    void CleanupUnusedTextures() override;
};

} // namespace Hyperion