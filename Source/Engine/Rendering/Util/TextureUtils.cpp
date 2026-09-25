/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/TextureUtils.hpp>

#include <algorithm>

namespace Hyperion {

namespace TextureUtils {

namespace {

void BuildAlphaHistogram(const ubyte* rgba, size_t numPixels, uint32 (&outHistogram)[256])
{
    std::fill(std::begin(outHistogram), std::end(outHistogram), 0u);

    for (size_t pixelIndex = 0; pixelIndex < numPixels; pixelIndex++)
    {
        outHistogram[rgba[pixelIndex * 4 + 3]]++;
    }
}

uint32 CountAlphaAtOrAbove(const uint32 (&histogram)[256], uint32 alphaByte)
{
    uint32 count = 0;

    for (uint32 value = alphaByte; value < 256; value++)
    {
        count += histogram[value];
    }

    return count;
}
} // namespace

void PreserveAlphaCoverage(const TextureDesc& desc, ByteBuffer& imageData, float alphaCutoff)
{
    static constexpr float Tolerance = 0.03f;

    const bool isRgba8 = desc.format == TextureFormat::RGBA8 || desc.format == TextureFormat::RGBA8_SRGB;

    if (!isRgba8 || !desc.HasStoredMips() || alphaCutoff <= 0.0f || alphaCutoff >= 1.0f)
    {
        return;
    }

    AssertDebug(imageData.Size() == desc.GetByteSize());

    const uint32 numMipLevels = desc.NumMips();
    const uint32 numArrayLayers = desc.NumArrayLayers();
    const uint32 cutoffByte = MathUtil::Clamp(uint32(MathUtil::Ceil(alphaCutoff * 255.0f)), 1u, 255u);

    uint32 histogram[256];

    for (uint32 layer = 0; layer < numArrayLayers; layer++)
    {
        const size_t baseLayerSize = desc.GetMipByteSize(0);
        const size_t baseNumPixels = baseLayerSize / 4;

        BuildAlphaHistogram(imageData.Data() + layer * baseLayerSize, baseNumPixels, histogram);

        const float targetCoverage = float(CountAlphaAtOrAbove(histogram, cutoffByte)) / float(baseNumPixels);

        if (targetCoverage <= 0.0f)
        {
            continue;
        }

        for (uint32 mip = 1; mip < numMipLevels; mip++)
        {
            const size_t mipLayerSize = desc.GetMipByteSize(uint8(mip));
            const size_t numPixels = mipLayerSize / 4;

            ubyte* mipLayerData = imageData.Data() + desc.mipOffsets[mip - 1] + layer * mipLayerSize;

            BuildAlphaHistogram(mipLayerData, numPixels, histogram);

            const float coverage = float(CountAlphaAtOrAbove(histogram, cutoffByte)) / float(numPixels);

            if (MathUtil::Abs(coverage - targetCoverage) <= Tolerance)
            {
                continue;
            }

            const uint32 targetCount = MathUtil::Max(uint32(MathUtil::Ceil(targetCoverage * float(numPixels))), 1u);

            uint32 thresholdByte = 1;
            uint32 passingCount = 0;

            for (uint32 alphaByte = 255; alphaByte >= 1; alphaByte--)
            {
                passingCount += histogram[alphaByte];

                if (passingCount >= targetCount)
                {
                    thresholdByte = alphaByte;
                    break;
                }
            }

            const float alphaScale = (float(cutoffByte) + 0.25f) / float(thresholdByte);

            for (size_t pixelIndex = 0; pixelIndex < numPixels; pixelIndex++)
            {
                ubyte& alpha = mipLayerData[pixelIndex * 4 + 3];
                alpha = ubyte(MathUtil::Min(float(alpha) * alphaScale + 0.5f, 255.0f));
            }
        }
    }
}

} // namespace TextureUtils

} // namespace Hyperion
