/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/TextureUtils.hpp>

#include <Core/Containers/Array.hpp>

#include <utility>

namespace Hyperion {

namespace TextureUtils {

void BuildAlphaCoverageMips(const TextureDesc& desc, ByteBuffer& imageData, float alphaCutoff)
{
    const bool isRgba8 = desc.format == TextureFormat::RGBA8 || desc.format == TextureFormat::RGBA8_SRGB;

    if (!isRgba8 || !desc.HasStoredMips() || desc.GetMipExtent(0).z != 1 || alphaCutoff <= 0.0f || alphaCutoff > 1.0f)
    {
        return;
    }

    AssertDebug(imageData.Size() == desc.GetByteSize());

    const uint32 numMipLevels = desc.NumMips();
    const uint32 numArrayLayers = desc.NumArrayLayers();
    const uint32 cutoffByte = MathUtil::Clamp(uint32(MathUtil::Ceil(alphaCutoff * 255.0f)), 1u, 255u);

    const Vec3u baseExtent = desc.GetMipExtent(0);
    const size_t baseLayerSize = desc.GetMipByteSize(0);

    Array<float> coverage;
    Array<float> nextCoverage;

    for (uint32 layer = 0; layer < numArrayLayers; layer++)
    {
        const ubyte* baseLayerData = imageData.Data() + layer * baseLayerSize;

        uint32 width = baseExtent.x;
        uint32 height = baseExtent.y;

        coverage.ResizeUninitialized(size_t(width) * size_t(height));

        for (size_t pixelIndex = 0; pixelIndex < coverage.Size(); pixelIndex++)
        {
            coverage[pixelIndex] = baseLayerData[pixelIndex * 4 + 3] >= cutoffByte ? 1.0f : 0.0f;
        }

        for (uint32 mip = 1; mip < numMipLevels; mip++)
        {
            const Vec3u mipExtent = desc.GetMipExtent(uint8(mip));
            const size_t mipLayerSize = desc.GetMipByteSize(uint8(mip));

            ubyte* mipLayerData = imageData.Data() + desc.mipOffsets[mip - 1] + layer * mipLayerSize;

            nextCoverage.ResizeUninitialized(size_t(mipExtent.x) * size_t(mipExtent.y));

            for (uint32 y = 0; y < mipExtent.y; y++)
            {
                const uint32 sourceRow0 = MathUtil::Min(y * 2, height - 1) * width;
                const uint32 sourceRow1 = MathUtil::Min(y * 2 + 1, height - 1) * width;

                for (uint32 x = 0; x < mipExtent.x; x++)
                {
                    const uint32 sourceColumn0 = MathUtil::Min(x * 2, width - 1);
                    const uint32 sourceColumn1 = MathUtil::Min(x * 2 + 1, width - 1);

                    const float value = 0.25f * (coverage[sourceRow0 + sourceColumn0] + coverage[sourceRow0 + sourceColumn1]
                        + coverage[sourceRow1 + sourceColumn0] + coverage[sourceRow1 + sourceColumn1]);

                    const size_t pixelIndex = size_t(y) * mipExtent.x + x;

                    nextCoverage[pixelIndex] = value;
                    mipLayerData[pixelIndex * 4 + 3] = ubyte(value * 255.0f + 0.5f);
                }
            }

            std::swap(coverage, nextCoverage);

            width = mipExtent.x;
            height = mipExtent.y;
        }
    }
}

} // namespace TextureUtils

} // namespace Hyperion
