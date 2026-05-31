/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/TextureViewCache.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/containers/Array.hpp>
#include <Core/containers/SparsePagedArray.hpp>
#include <Core/containers/Map.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

class DX12TextureViewCache final : public TextureViewCacheBase
{
public:
    struct SubtypeData
    {
        SparsePagedArray<TMap<uint64, DX12GpuImageViewRef, DX12Allocator>, 32, DX12Allocator> imageViews;
        SparsePagedArray<WeakHandle<Texture>, 32, DX12Allocator> weakTextureHandles;
    };

    SharedMutex mutex;
    Array<SubtypeData, DX12Allocator> subtypeImpls;

    DX12TextureViewCache();

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

private:
    SubtypeData& GetSubtypeData(ObjId<Texture> id);
};

} // namespace Hyperion
