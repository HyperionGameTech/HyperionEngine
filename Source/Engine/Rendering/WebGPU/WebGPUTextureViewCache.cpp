/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUTextureViewCache.hpp>
#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>

#include <Rendering/Texture.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

namespace Hyperion {

extern WebGPURenderInterface RI;

static constexpr uint64 CalculateImageViewHash(const ImageSubResource& subResource, TextureType viewTextureType)
{
    return subResource.GetHashCode()
        .Combine(viewTextureType)
        .Value();
}

WebGPUTextureViewCache::WebGPUTextureViewCache()
{
    const size_t numSubtypes = GetNumDescendants(TypeId::ForType<Texture>()) + 1;
    subtypeImpls.Resize(numSubtypes);
}

WebGPUTextureViewCache::~WebGPUTextureViewCache()
{
    for (auto& subtype : subtypeImpls)
    {
        for (auto& it : subtype.imageViews)
        {
            for (auto& jt : it)
            {
                jt.second.Reset();
            }
        }
    }
}

WebGPUTextureViewCache::SubtypeData& WebGPUTextureViewCache::GetSubtypeData(ObjId<Texture> id)
{
    const int classIndex = GetSubclassIndex(TypeId::ForType<Texture>(), id.GetTypeId()) + 1;
    AssertDebug(classIndex >= 0, "Invalid class index {}", classIndex);
    AssertDebug(classIndex < int(subtypeImpls.Size()), "Invalid class index {}", classIndex);

    return subtypeImpls[classIndex];
}

const WebGPUGpuImageViewRef& WebGPUTextureViewCache::GetOrCreate(
    Texture* texture,
    uint32 mipIndex,
    uint32 numMips,
    uint32 layerIndex,
    uint32 numLayers)
{
    if (!texture)
    {
        return WebGPUGpuImageViewRef::Null();
    }

    WebGPUGpuImage* gpuImage = texture->GetGpuImage();
    AssertDebug(gpuImage != nullptr);

    const TextureDesc& textureDesc = gpuImage->GetTextureDesc();

    const uint32 maxMipLevel = textureDesc.NumMips() - 1;
    const uint32 maxArrayLayer = textureDesc.NumArrayLayers() - 1;

    ImageSubResource subResource {};
    subResource.numLevels = MathUtil::Min(numMips, maxMipLevel + 1);
    subResource.baseMipLevel = MathUtil::Min(mipIndex, maxMipLevel);
    subResource.numLayers = MathUtil::Min(numLayers, maxArrayLayer + 1);
    subResource.baseArrayLayer = MathUtil::Min(layerIndex, maxArrayLayer);

    return GetOrCreate(texture, subResource, textureDesc.type);
}

const WebGPUGpuImageViewRef& WebGPUTextureViewCache::GetOrCreate(
    Texture* texture, const ImageSubResource& subResource)
{
    Assert(texture != nullptr);

    WebGPUGpuImage* gpuImage = texture->GetGpuImage();
    AssertDebug(gpuImage != nullptr);

    const TextureDesc& textureDesc = gpuImage->GetTextureDesc();

    return GetOrCreate(texture, subResource, textureDesc.type);
}

const WebGPUGpuImageViewRef& WebGPUTextureViewCache::GetOrCreate(
    Texture* texture,
    const ImageSubResource& subResource,
    TextureType viewTextureType)
{
    Assert(texture != nullptr);

    const size_t idx = texture->Id().ToIndex();

    TSharedLock sharedLock(mutex);
    Optional<TUniqueLock<SharedMutex>> uniqueLockOpt;

    SubtypeData& subtypeData = GetSubtypeData(texture->Id());

    if (!subtypeData.imageViews.HasIndex(idx))
    {
        // Upgrade lock. We need it to mutate these
        sharedLock.Reset();
        uniqueLockOpt.Emplace(mutex);

        subtypeData.imageViews.Emplace(idx);
        subtypeData.weakTextureHandles.Emplace(idx, MakeWeakRef(texture));
    }

    auto& textureImageViews = subtypeData.imageViews.Get(idx);

    const uint64 key = CalculateImageViewHash(subResource, viewTextureType);

    auto it = textureImageViews.Find(key);

    if (it == textureImageViews.End())
    {
        // Upgrade lock if not already 
        if (!uniqueLockOpt.HasValue())
        {
            sharedLock.Reset();
            uniqueLockOpt.Emplace(mutex);
        }

        // Re-check now that we hold the exclusive lock -- another thread may have raced us
        // and already inserted this entry while we were waiting on the shared -> unique upgrade.
        it = textureImageViews.Find(key);

        if (it == textureImageViews.End())
        {
            WebGPUGpuImageViewRef imageView = MakeHandle<WebGPUGpuImageView>(
                texture->GetGpuImage(), subResource, viewTextureType);

            Check(imageView->Create());

            it = textureImageViews.Set(key, imageView).first;
        }
    }

    Assert(it->second.IsValid());

    return it->second;
}

void WebGPUTextureViewCache::RemoveTexture(const Texture* texture)
{
    if (!texture)
    {
        return;
    }

    const size_t idx = texture->Id().ToIndex();

    TUniqueLock lock(mutex);

    SubtypeData& subtypeData = GetSubtypeData(texture->Id());

    if (subtypeData.imageViews.HasIndex(idx))
    {
        for (auto& it : subtypeData.imageViews.Get(idx))
        {
            EnqueueDeletion(std::move(it.second));
        }

        subtypeData.imageViews.EraseAt(idx);
        subtypeData.weakTextureHandles.EraseAt(idx);
    }
}

void WebGPUTextureViewCache::OnFrameEnd(uint32 prevFrameIndex)
{
    AssertOnThread(g_renderThread);

    TUniqueLock lock(mutex);

    constexpr uint32 MaxCycles = 32;

    uint32 numRemoved = 0;

    for (auto& subtype : subtypeImpls)
    {
        auto it = subtype.weakTextureHandles.Begin();

        for (uint32 i = 0; it != subtype.weakTextureHandles.End() && numRemoved < MaxCycles; i++)
        {
            if (it->Expired())
            {
                const size_t idx = subtype.weakTextureHandles.IndexOf(it);

                Assert(subtype.imageViews.HasIndex(idx));

                for (auto& jt : subtype.imageViews.Get(idx))
                {
                    EnqueueDeletion(std::move(jt.second));
                }

                subtype.imageViews.EraseAt(idx);

                it = subtype.weakTextureHandles.Erase(it);

                ++numRemoved;

                continue;
            }

            ++it;
        }

        if (numRemoved >= MaxCycles)
        {
            break;
        }
    }

    if (numRemoved != 0)
    {
        HYP_LOG(RenderingBackend, Verbose, "WebGPUTextureViewCache: Cleaned up {} unused textures", numRemoved);
    }
}

} // namespace Hyperion
