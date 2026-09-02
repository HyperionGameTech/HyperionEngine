/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/ScratchImageAllocator.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Core/Reflection/Enum.hpp>

namespace Hyperion {

#pragma region ScratchImageAllocator

static constexpr bool UseNextPowerOfTwoExtent = true;
static constexpr uint32 MaxFramesBeforeDiscard = 100;

struct ScratchImageAllocatorImpl
{
    struct CachedScratchImage
    {
        TextureType type;
        TextureFormat format;
        Vec3u extent;
        Vec3u alignedExtent;
        uint32 lastUsedFrame;
        Handle<Texture> texture;

        // cachedImages is kept sorted by this ordering so AcquireScratchImage() can LowerBound()
        // straight to the first plausible (type, format) group instead of scanning the whole cache.
        HYP_FORCE_INLINE bool operator<(const CachedScratchImage& other) const
        {
            if (type != other.type)
            {
                return type < other.type;
            }

            if (format != other.format)
            {
                return format < other.format;
            }

            return alignedExtent.Volume() < other.alignedExtent.Volume();
        }
    };

    Array<CachedScratchImage, RenderAllocator> cachedImages;
    List<CachedScratchImage, RenderAllocator> usedImages;

    SharedMutex mutex;

    uint32 frameIndex = 0;

    ~ScratchImageAllocatorImpl() = default;

    void OnFrameStart(uint32 newFrameIndex)
    {
        frameIndex = newFrameIndex;
    }

    void OnFrameEnd(uint32 prevFrameIndex)
    {
        for (auto it = cachedImages.Begin(); it != cachedImages.End();)
        {
            CachedScratchImage& cachedImage = *it;

            if (int64(prevFrameIndex) - int64(cachedImage.lastUsedFrame) >= MaxFramesBeforeDiscard)
            {
                it = cachedImages.Erase(it);

                continue;
            }

            ++it;
        }

        // usedImages is populated in strictly non-decreasing lastUsedFrame order (frameIndex only
        // increases across acquisitions), so we can stop at the first entry that isn't old enough
        // yet instead of scanning the whole list.
        while (!usedImages.Empty())
        {
            CachedScratchImage& usedImage = usedImages.Front();

            if (int64(prevFrameIndex) - int64(usedImage.lastUsedFrame) < int64(NumFramesInFlight))
            {
                break;
            }

            auto lowerBoundIt = cachedImages.LowerBound(usedImage);
            cachedImages.Insert(lowerBoundIt, std::move(usedImage));

            usedImages.PopFront();
        }
    }

    Handle<Texture> AcquireScratchImage(TextureType type, TextureFormat format, Vec3u extent)
    {
        AssertDebug(extent.x > 0 && extent.y > 0 && extent.z > 0);

        const Vec3u alignedExtent = UseNextPowerOfTwoExtent
            ? Vec3u { uint32(MathUtil::NextPowerOf2(extent.x)), uint32(MathUtil::NextPowerOf2(extent.y)), uint32(MathUtil::NextPowerOf2(extent.z)) }
            : extent;

        TUniqueLock lock(mutex);

        CachedScratchImage searchKey {};
        searchKey.type = type;
        searchKey.format = format;
        searchKey.alignedExtent = alignedExtent;

        // cachedImages is sorted by (type, format, volume) - LowerBound() jumps straight to the
        // first entry that could possibly satisfy this request.
        for (auto it = cachedImages.LowerBound(searchKey); it != cachedImages.End(); ++it)
        {
            if (it->type != type || it->format != format)
            {
                // Past this (type, format) group entirely - no later entry can match either.
                break;
            }

            if (it->alignedExtent.x >= alignedExtent.x
                && it->alignedExtent.y >= alignedExtent.y
                && it->alignedExtent.z >= alignedExtent.z)
            {
                CachedScratchImage entry = std::move(*it);
                entry.lastUsedFrame = frameIndex;

                cachedImages.Erase(it);

                Handle<Texture> texture = entry.texture;
                usedImages.PushBack(std::move(entry));

                return texture;
            }
        }

        CachedScratchImage& newEntry = usedImages.EmplaceBack();

        newEntry = {};
        newEntry.lastUsedFrame = frameIndex;
        newEntry.type = type;
        newEntry.format = format;
        newEntry.extent = extent;
        newEntry.alignedExtent = alignedExtent;

        newEntry.texture = MakeHandle<Texture>(TextureDesc {
            type,
            format,
            alignedExtent,
            TFM_LINEAR_MIPMAP,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE
        });

        newEntry.texture->SetIsTransient(true);
        newEntry.texture->SetName(NAME_FMT("ScratchImg_{}_{}_{}x{}x{}", EnumToString(type), EnumToString(format), extent.x, extent.y, extent.z));

        RendererResult createResult = newEntry.texture->Create();
        if (!createResult)
        {
            HYP_LOG(RenderingBackend, Error, "ScratchImageAllocator: Failed to create scratch texture: {}",
                createResult.HasError() ? createResult.GetError().GetMessage() : "Unknown error");

            usedImages.PopBack();
            return {};
        }

        return newEntry.texture;
    }

    void Shutdown()
    {
        for (auto& cached : cachedImages)
        {
            cached.texture.Reset();
        }

        cachedImages.Clear();

        for (auto& used : usedImages)
        {
            used.texture.Reset();
        }

        usedImages.Clear();
    }
};

ScratchImageAllocator::ScratchImageAllocator()
    : m_impl(MakePimplWithAllocator<ScratchImageAllocatorImpl, RenderAllocator>())
{
}

ScratchImageAllocator::~ScratchImageAllocator()
{
    Shutdown();
}

void ScratchImageAllocator::OnFrameStart(uint32 newFrameIndex)
{
    m_impl->OnFrameStart(newFrameIndex);
}

void ScratchImageAllocator::OnFrameEnd(uint32 prevFrameIndex)
{
    m_impl->OnFrameEnd(prevFrameIndex);
}

Handle<Texture> ScratchImageAllocator::AcquireScratchImage(TextureType type, TextureFormat format, Vec3u extent)
{
    return m_impl->AcquireScratchImage(type, format, extent);
}

void ScratchImageAllocator::Shutdown()
{
    m_impl->Shutdown();
}

#pragma endregion ScratchImageAllocator

} // namespace Hyperion
