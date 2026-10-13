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

#include <Core/Containers/Array.hpp>
#include <Core/Containers/SparsePagedArray.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

class WebGPUTextureViewCache final : public TextureViewCacheBase
{
public:
    struct SubtypeData
    {
        SparsePagedArray<Map<uint64, WebGPUGpuImageViewRef, WebGPUAllocator>, 32, WebGPUAllocator> imageViews;
        SparsePagedArray<WeakHandle<Texture>, 32, WebGPUAllocator> weakTextureHandles;
    };

    SharedMutex mutex;
    Array<SubtypeData, WebGPUAllocator> subtypeImpls;

    WebGPUTextureViewCache();

    ~WebGPUTextureViewCache() override;

    const WebGPUGpuImageViewRef& GetOrCreate(
        Texture* texture,
        uint32 mipIndex = 0,
        uint32 numMips = ~0u,
        uint32 layerIndex = 0,
        uint32 numLayers = ~0u) override;

    const WebGPUGpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource) override;

    const WebGPUGpuImageViewRef& GetOrCreate(
        Texture* texture,
        const ImageSubResource& subResource,
        TextureType viewTextureType) override;

    void RemoveTexture(const Texture* texture) override;
    void OnFrameEnd(uint32 prevFrameIndex) override;

private:
    SubtypeData& GetSubtypeData(ObjId<Texture> id);
};

} // namespace Hyperion
