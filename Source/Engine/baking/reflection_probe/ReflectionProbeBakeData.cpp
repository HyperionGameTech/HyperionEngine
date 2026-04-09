/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/reflection_probe/ReflectionProbeBakeData.hpp>

#include <rendering/Texture.hpp>

#include <scene/EnvProbe.hpp>

namespace Hyperion {

namespace Baking {

Result BakeData<ReflectionProbe>::Build()
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
        const Vec3f up = Texture::s_cubemapDirections[face].second * Vec3f(-1.0f);
        const Vec3f right = forward.Cross(up).Normalize();

        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * uint32(numTexelsPerFace) + y * dimensions.x + x;

                const float u = (float(x) + 0.5f) / float(dimensions.x) * 2.0f - 1.0f;
                const float v = (float(y) + 0.5f) / float(dimensions.y) * 2.0f - 1.0f;

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

auto BakeData<ReflectionProbe>::ToBitmap() const -> BitmapType
{
    Assert(m_envProbe != nullptr);

    const size_t numTexelsPerFace = dimensions.x * dimensions.y;

    Assert(texels.Size() == 6 * numTexelsPerFace, "Invalid cubemap size");

    BitmapType bitmap(dimensions.x, dimensions.y * 6);

    for (uint32 face = 0; face < 6; face++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * numTexelsPerFace + y * dimensions.x + x;
                const uint32 bitmapY = face * dimensions.y + y;

                Vec4f color = texels[texelIdx].color0;

                if (color.w <= 0.0f)
                {
                    continue;
                }

                color /= color.w;

                AssertDebug(!MathUtil::IsNaN(color));

                bitmap.GetPixelReference(x, bitmapY).SetRGBA(color);
            }
        }
    }

    return bitmap;
}

} // namespace Baking

} // namespace Hyperion
