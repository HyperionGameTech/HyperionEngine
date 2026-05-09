/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/TextureViewCache.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/containers/SparsePagedArray.hpp>
#include <Core/containers/Map.hpp>

#include <Core/threading/SharedMutex.hpp>

namespace Hyperion {

class VulkanTextureViewCache final : public TextureViewCacheBase
{
public:
    using TextureImageViewMap = TMap<uint64, VulkanGpuImageViewRef, VulkanAllocator>;

    struct SubtypeData
    {
        SparsePagedArray<TextureImageViewMap, 32, VulkanAllocator> imageViews;
        SparsePagedArray<WeakHandle<Texture>, 32, VulkanAllocator> weakTextureHandles;
    };

    SharedMutex mutex;
    Array<SubtypeData, VulkanAllocator> subtypeImpls;

    VulkanTextureViewCache();

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

private:
    SubtypeData& GetSubtypeData(ObjId<Texture> id);
};

} // namespace Hyperion
