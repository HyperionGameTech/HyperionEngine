/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/EnvProbe/EnvProbeBakeData.hpp>

#include <Rendering/Texture.hpp>

#include <Scene/EnvProbe.hpp>

namespace Hyperion {

namespace Baking {

Result BakeData<EnvProbe>::Build()
{
    Assert(m_envProbe != nullptr);

    // texels need to be 6*resolution^2 in size
    dimensions = Vec3u(m_envProbe->GetDimensions(), 1);

    AssertDebug(dimensions.Volume() > 0 && dimensions.x == dimensions.y,
                "EnvProbe lightmap dimensions must be square and non-zero! Dimensions: {}", dimensions);

    const uint32 numTexelsPerFace = dimensions.x * dimensions.y;
    const uint32 numTexelsTotal = 6 * numTexelsPerFace;

    texels.Resize(numTexelsTotal);
    m_rays.Resize(numTexelsTotal);

    const Vec3f origin = m_envProbe->GetWorldTranslation();

    for (uint32 face = 0; face < 6; face++)
    {
        const Vec3f forward = Texture::s_cubemapDirections[face].first;
        const Vec3f up = Texture::s_cubemapDirections[face].second;
        const Vec3f right = up.Cross(forward).Normalize();

        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * uint32(numTexelsPerFace) + y * dimensions.x + x;

                const float u = (float(x) + 0.5f) / float(dimensions.x) * 2.0f - 1.0f;
                const float v = 1.0 - (float(y) + 0.5f) / float(dimensions.y) * 2.0f;

                const Vec3f dir = (forward + right * u + up * v).Normalize();

                LightmapRay& ray = m_rays[texelIdx];
                ray = LightmapRay {
                    Ray { origin, dir },
                    /* meshId */ ObjId<Mesh>::invalid,
                    /* triangleIndex */ ~0u,
                    /* texelIndex */ texelIdx
                };

                LightmapTexel& texel = texels[texelIdx];
                texel.pRay = &ray;
            }
        }
    }

    return {};
}

auto BakeData<EnvProbe>::ToBitmap() const -> BitmapType
{
    Assert(m_envProbe != nullptr);

    const size_t numTexelsPerFace = dimensions.x * dimensions.y;

    Assert(texels.Size() == 6 * numTexelsPerFace, "Invalid cubemap size");

    BitmapType bitmap(dimensions.x, dimensions.y * 6);

    for (uint32 face = 0; face < 6; face++)
    {
        uint32 numSkipped = 0;
        uint32 numLowAlpha = 0;
        double sumW = 0.0;
        double sumPreRgb = 0.0;
        double sumPostRgb = 0.0;
        uint32 numProcessed = 0;

        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * numTexelsPerFace + y * dimensions.x + x;
                const uint32 bitmapY = face * dimensions.y + y;

                Vec4f color = texels[texelIdx].color0;

                if (color.w <= 0.00001f)
                {
                    numSkipped++;

                    continue;
                }

                const float preRgbLen = MathUtil::Max(color.x, MathUtil::Max(color.y, color.z));

                if (color.w < 0.3f)
                {
                    numLowAlpha++;
                }

                color /= color.w;

                color = MathUtil::Max(color, Vec4f::Zero());

                sumW += color.w;
                sumPreRgb += preRgbLen;
                sumPostRgb += MathUtil::Max(color.x, MathUtil::Max(color.y, color.z));
                numProcessed++;

                if constexpr (!BitmapType::Helper::IsFloatingPoint)
                {
                    color = MathUtil::Min(color, Vec4f::One());
                }

                AssertDebug(!MathUtil::IsNaN(color));

                bitmap.GetPixelReference(x, bitmapY).SetRGBA(color);
            }
        }

        HYP_LOG(Lightmap, Info, "[EnvProbe Bake] face {} stats: skipped {}/{}, lowAlpha(<0.3) {}, meanW {}, meanPreRGB {}, meanPostRGB {}",
            face,
            numSkipped,
            uint32(numTexelsPerFace),
            numLowAlpha,
            numProcessed ? MathUtil::Round(float(sumW / numProcessed), 4) : 0.0f,
            numProcessed ? MathUtil::Round(float(sumPreRgb / numProcessed), 4) : 0.0f,
            numProcessed ? MathUtil::Round(float(sumPostRgb / numProcessed), 4) : 0.0f);
    }

    return bitmap;
}

auto BakeData<EnvProbe>::ToVisibilityBitmap() const -> VisibilityBitmapType
{
    Assert(m_envProbe != nullptr);

    const size_t numTexelsPerFace = dimensions.x * dimensions.y;

    Assert(texels.Size() == 6 * numTexelsPerFace, "Invalid cubemap size");

    // The visibility texture must match the global envProbesDepthTexture dimensions
    // (see OnBindingChanged_EnvProbe which CopyImages into that array).
    static constexpr uint32 VisibilityTextureDimension = 16;

    VisibilityBitmapType bitmap(VisibilityTextureDimension, VisibilityTextureDimension * 6);

    for (uint32 face = 0; face < 6; face++)
    {
        for (uint32 y = 0; y < VisibilityTextureDimension; y++)
        {
            for (uint32 x = 0; x < VisibilityTextureDimension; x++)
            {
                const uint32 bitmapY = face * VisibilityTextureDimension + y;

                // Accumulate distance moments over the region of cubemap texels
                // that map to this visibility texel.
                float accumDist = 0.0f;
                float accumDistSq = 0.0f;
                float accumWeight = 0.0f;

                const uint32 xStart = (x * dimensions.x) / VisibilityTextureDimension;
                const uint32 xEnd = MathUtil::Max(xStart + 1, ((x + 1) * dimensions.x) / VisibilityTextureDimension);
                const uint32 yStart = (y * dimensions.y) / VisibilityTextureDimension;
                const uint32 yEnd = MathUtil::Max(yStart + 1, ((y + 1) * dimensions.y) / VisibilityTextureDimension);

                for (uint32 sy = yStart; sy < yEnd; sy++)
                {
                    for (uint32 sx = xStart; sx < xEnd; sx++)
                    {
                        const uint32 texelIdx = face * numTexelsPerFace + sy * dimensions.x + sx;

                        // color1 holds accumulated (dist, dist^2, 0, sample_count) from the DISTANCE pass.
                        const Vec4f accum = texels[texelIdx].color1;

                        if (accum.w > 0.0f)
                        {
                            accumDist += accum.x / accum.w;
                            accumDistSq += accum.y / accum.w;
                            accumWeight += 1.0f;
                        }
                        else
                        {
                            static constexpr float MissDist = 10000.0f;

                            accumDist += MissDist;
                            accumDistSq += MissDist * MissDist;
                            accumWeight += 1.0f;
                        }
                    }
                }

                Vec2f moments = Vec2f::Zero();

                if (accumWeight > 0.0f)
                {
                    moments.x = accumDist / accumWeight;
                    moments.y = accumDistSq / accumWeight;
                }

                AssertDebug(!MathUtil::IsNaN(moments));

                bitmap.GetPixelReference(x, bitmapY).SetRG(moments);
            }
        }
    }

    return bitmap;
}

} // namespace Baking

} // namespace Hyperion
