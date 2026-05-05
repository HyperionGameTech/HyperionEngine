/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <rendering/ScratchImageAllocator.hpp>
#include <rendering/RenderInterface.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <Core/reflection/Enum.hpp>

namespace Hyperion {

#pragma region ScratchImageAllocator

static constexpr bool UseNextPowerOfTwoExtent = true;
static constexpr uint32 MaxFramesBeforeDiscard = NumFramesInFlight;

struct ScratchImageAllocatorImpl
{
    struct CachedScratchImage
    {
        TextureFormat format = TextureFormat::RGBA8;
        Vec3u extent = Vec3u::One();
        Vec3u alignedExtent = Vec3u::One();
        uint32 lastUsedFrame = 0;
        Handle<Texture> texture;
    };

    LinkedList<CachedScratchImage, RenderAllocator> cachedImages;
    LinkedList<CachedScratchImage, RenderAllocator> usedImages;

    SharedMutex mutex;

    ~ScratchImageAllocatorImpl() = default;

    void OnFrameStart()
    {
    }

    void OnFrameEnd()
    {
        const uint32 frameCounter = GetFrameCounter();

        for (auto it = cachedImages.Begin(); it != cachedImages.End();)
        {
            CachedScratchImage& cachedImage = *it;

            if (int64(frameCounter) - int64(cachedImage.lastUsedFrame) >= MaxFramesBeforeDiscard)
            {
                it = cachedImages.Erase(it);

                continue;
            }

            ++it;
        }

        for (auto it = usedImages.Begin(); it != usedImages.End();)
        {
            CachedScratchImage& usedImage = *it;

            cachedImages.PushBack(std::move(usedImage));

            it = usedImages.Erase(it);
        }
    }

    Handle<Texture> AcquireScratchImage(TextureFormat format, Vec3u extent)
    {
        AssertDebug(extent.x > 0 && extent.y > 0 && extent.z > 0);

        const Vec3u alignedExtent = UseNextPowerOfTwoExtent
            ? Vec3u { uint32(MathUtil::NextPowerOf2(extent.x)), uint32(MathUtil::NextPowerOf2(extent.y)), uint32(MathUtil::NextPowerOf2(extent.z)) }
            : extent;

        TUniqueLock lock(mutex);

        for (auto it = cachedImages.Begin(); it != cachedImages.End(); ++it)
        {
            if (it->format == format
                && it->alignedExtent.x >= alignedExtent.x
                && it->alignedExtent.y >= alignedExtent.y
                && it->alignedExtent.z >= alignedExtent.z)
            {
                CachedScratchImage& entry = usedImages.PushBack(std::move(*it));
                entry.lastUsedFrame = GetFrameCounter();

                cachedImages.Erase(it);

                return entry.texture;
            }
        }

        CachedScratchImage& newEntry = usedImages.EmplaceBack();

        newEntry.lastUsedFrame = GetFrameCounter();
        newEntry.format = format;
        newEntry.extent = extent;
        newEntry.alignedExtent = alignedExtent;

        newEntry.texture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            format,
            alignedExtent,
            TFM_LINEAR_MIPMAP,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE
        });

        newEntry.texture->SetIsTransient(true);
        newEntry.texture->SetName(NAME_FMT("ScratchImg_{}_{}_{}_{}", extent.x, extent.y, extent.z, EnumToString(format)));

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
    : m_impl(MakePimpl<ScratchImageAllocatorImpl>())
{
}

ScratchImageAllocator::~ScratchImageAllocator()
{
    Shutdown();
}

void ScratchImageAllocator::OnFrameStart()
{
    m_impl->OnFrameStart();
}

void ScratchImageAllocator::OnFrameEnd()
{
    m_impl->OnFrameEnd();
}

Handle<Texture> ScratchImageAllocator::AcquireScratchImage(TextureFormat format, Vec3u extent)
{
    return m_impl->AcquireScratchImage(format, extent);
}

void ScratchImageAllocator::Shutdown()
{
    m_impl->Shutdown();
}

#pragma endregion ScratchImageAllocator

} // namespace Hyperion
