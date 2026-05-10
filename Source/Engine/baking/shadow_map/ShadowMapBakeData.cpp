/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/shadow_map/ShadowMapBakeData.hpp>

#include <scene/Light.hpp>

#include <rendering/Texture.hpp>

namespace Hyperion {

namespace Baking {

uint32 BakeData<Light>::GetNumFaces() const
{
    if (m_light && m_light->IsA(PointLight::StaticClass()))
    {
        return 6;
    }

    return 1;
}

Result BakeData<Light>::Build()
{
    Assert(m_light != nullptr);

    dimensions = Vec3u(m_light->GetShadowMapDimensions(), 1);

    AssertDebug(dimensions.Volume() > 0 && dimensions.x == dimensions.y,
        "EnvProbe lightmap dimensions must be square and non-zero! Dimensions: {}", dimensions);

    const uint32 numTexelsPerFace = dimensions.x * dimensions.y;

    const uint32 numFaces = GetNumFaces();
    const uint32 numTexelsTotal = numFaces * numTexelsPerFace;

    texels.Resize(numTexelsTotal);
    m_rays.Resize(numTexelsTotal);

    m_projMat = Mat4f::Perspective(90.0f, dimensions.x, dimensions.y, 0.01f, 1000.0f);
    m_projMat[1][1] = -m_projMat[1][1];

    const Vec3f origin = m_light->GetWorldTranslation();

    for (uint32 face = 0; face < numFaces; face++)
    {
        const Vec3f forward = Texture::s_cubemapDirections[face].first;
        const Vec3f up = Texture::s_cubemapDirections[face].second * Vec3f(-1.0f);
        const Vec3f right = forward.Cross(up).Normalize();

        m_viewProjMats[face] = m_projMat * Mat4f::LookAt(origin, origin + forward, up);

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

auto BakeData<Light>::ToBitmap() const -> BitmapType
{
    Assert(m_light != nullptr);

    const uint32 numFaces = GetNumFaces();
    const size_t numTexelsPerFace = dimensions.x * dimensions.y;

    Assert(texels.Size() == numFaces * numTexelsPerFace, "Invalid cubemap size");

    BitmapType bitmap(dimensions.x, dimensions.y * 6);

    for (uint32 face = 0; face < 6; face++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * numTexelsPerFace + y * dimensions.x + x;
                const uint32 bitmapY = face * dimensions.y + y;

                const Vec4f& color = texels[texelIdx].color0;
                const float dist = color.GetX();

                // no hit
                if (dist < 0.0001f)
                {
                    bitmap.GetPixelReference(x, bitmapY).SetComponentRaw(0, UINT16_MAX);
                    continue;
                }

                // otherwise transform to 0..1 range based on viewproj mat
                //const Vec4f transformedPosition = (m_viewProjMats[face] * Vec4f(texels[texelIdx].pRay->ray.position + texels[texelIdx].pRay->ray.direction * dist, 1.0f));

                // otherwise transform to 0..1 range based on viewproj mat
                const Vec4f transformedPosition = (m_projMat * Vec4f(0.0f, 0.0f, dist, 1.0f));
                const float depth = transformedPosition.z / transformedPosition.w;

                bitmap.GetPixelReference(x, bitmapY).SetComponentRaw(0, uint16(depth * float(UINT16_MAX)));
            }
        }
    }

    return bitmap;
}

} // namespace Baking

} // namespace Hyperion
