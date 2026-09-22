/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#ifdef HYP_EDITOR

#include <Scene/Systems/Editor/EditorIcons.hpp>

#include <Rendering/Texture.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Containers/Map.hpp>

#include <cmath>

namespace Hyperion {
namespace EditorIcons {

static SdfCanvas BuildPointLight()
{
    static const Vec2f s_neck[] = { Vec2f(-0.3f, -0.05f), Vec2f(0.3f, -0.05f), Vec2f(0.2f, -0.42f), Vec2f(-0.2f, -0.42f) };

    SdfCanvas canvas;
    canvas.Circle(Vec2f(0.0f, 0.25f), 0.5f)
        .ConvexPolygon(s_neck)
        .Segment(Vec2f(-0.18f, -0.56f), Vec2f(0.18f, -0.56f), 0.07f)
        .Segment(Vec2f(-0.1f, -0.73f), Vec2f(0.1f, -0.73f), 0.07f);

    return canvas;
}

static SdfCanvas BuildSpotLight()
{
    static const Vec2f s_housing[] = { Vec2f(-0.2f, 0.82f), Vec2f(0.2f, 0.82f), Vec2f(0.46f, 0.22f), Vec2f(-0.46f, 0.22f) };

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_housing)
        .Segment(Vec2f(-0.34f, -0.02f), Vec2f(-0.6f, -0.55f), 0.07f)
        .Segment(Vec2f(0.0f, -0.06f), Vec2f(0.0f, -0.62f), 0.07f)
        .Segment(Vec2f(0.34f, -0.02f), Vec2f(0.6f, -0.55f), 0.07f);

    return canvas;
}

static SdfCanvas BuildDirectionalLight()
{
    SdfCanvas canvas;
    canvas.Circle(Vec2f::Zero(), 0.34f);

    for (int rayIndex = 0; rayIndex < 8; rayIndex++)
    {
        const float angle = float(rayIndex) * MathUtil::pi<float> * 0.25f;
        const Vec2f direction = Vec2f(std::cos(angle), std::sin(angle));

        canvas.Segment(direction * 0.52f, direction * 0.8f, 0.07f);
    }

    return canvas;
}

static SdfCanvas BuildAreaLight()
{
    SdfCanvas canvas;
    canvas.RoundedBox(Vec2f(0.0f, 0.4f), Vec2f(0.78f, 0.22f), 0.06f);

    for (float rayX : { -0.5f, 0.0f, 0.5f })
    {
        canvas.Segment(Vec2f(rayX, -0.04f), Vec2f(rayX, -0.55f), 0.07f);
    }

    return canvas;
}

static SdfCanvas BuildCamera()
{
    static const Vec2f s_lens[] = { Vec2f(0.3f, -0.08f), Vec2f(0.88f, 0.18f), Vec2f(0.88f, -0.58f), Vec2f(0.3f, -0.32f) };

    SdfCanvas canvas;
    canvas.RoundedBox(Vec2f(-0.18f, -0.2f), Vec2f(0.52f, 0.34f), 0.08f)
        .ConvexPolygon(s_lens)
        .Circle(Vec2f(-0.45f, 0.42f), 0.22f)
        .Circle(Vec2f(0.08f, 0.42f), 0.22f);

    return canvas;
}

static SdfCanvas BuildEnvProbe()
{
    SdfCanvas canvas;
    canvas.Ring(Vec2f::Zero(), 0.72f, 0.07f)
        .EllipseRing(Vec2f::Zero(), Vec2f(0.72f, 0.26f), 0.055f)
        .EllipseRing(Vec2f::Zero(), Vec2f(0.26f, 0.72f), 0.055f);

    return canvas;
}

static SdfCanvas BuildLightmapVolume()
{
    static const Vec2f s_front[] = { Vec2f(-0.62f, -0.7f), Vec2f(0.28f, -0.7f), Vec2f(0.28f, 0.2f), Vec2f(-0.62f, 0.2f) };
    static const Vec2f s_backOffset = Vec2f(0.34f, 0.42f);

    static constexpr float s_edgeRadius = 0.055f;

    SdfCanvas canvas;

    for (int cornerIndex = 0; cornerIndex < 4; cornerIndex++)
    {
        const Vec2f& front = s_front[cornerIndex];
        const Vec2f& frontNext = s_front[(cornerIndex + 1) % 4];

        canvas.Segment(front, frontNext, s_edgeRadius)
            .Segment(front + s_backOffset, frontNext + s_backOffset, s_edgeRadius)
            .Segment(front, front + s_backOffset, s_edgeRadius);
    }

    return canvas;
}

static SdfCanvas BuildNode()
{
    SdfCanvas canvas;
    canvas.Ring(Vec2f::Zero(), 0.62f, 0.08f)
        .Circle(Vec2f::Zero(), 0.2f);

    return canvas;
}

static SdfCanvas BuildEntity()
{
    static const Vec2f s_top = Vec2f(0.0f, 0.82f);
    static const Vec2f s_upperRight = Vec2f(0.71f, 0.41f);
    static const Vec2f s_lowerRight = Vec2f(0.71f, -0.41f);
    static const Vec2f s_bottom = Vec2f(0.0f, -0.82f);
    static const Vec2f s_lowerLeft = Vec2f(-0.71f, -0.41f);
    static const Vec2f s_upperLeft = Vec2f(-0.71f, 0.41f);
    static const Vec2f s_center = Vec2f::Zero();

    static const Vec2f s_topFace[] = { s_top, s_upperRight, s_center, s_upperLeft };
    static const Vec2f s_leftFace[] = { s_upperLeft, s_center, s_bottom, s_lowerLeft };
    static const Vec2f s_rightFace[] = { s_center, s_upperRight, s_lowerRight, s_bottom };

    static constexpr float s_seamInset = 0.035f;

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_topFace, s_seamInset)
        .ConvexPolygon(s_leftFace, s_seamInset)
        .ConvexPolygon(s_rightFace, s_seamInset);

    return canvas;
}

static SdfCanvas& AddPictureFrame(SdfCanvas& canvas)
{
    static const Vec2f s_corners[] = { Vec2f(-0.76f, -0.62f), Vec2f(0.76f, -0.62f), Vec2f(0.76f, 0.62f), Vec2f(-0.76f, 0.62f) };

    for (int cornerIndex = 0; cornerIndex < 4; cornerIndex++)
    {
        canvas.Segment(s_corners[cornerIndex], s_corners[(cornerIndex + 1) % 4], 0.07f);
    }

    return canvas;
}

static SdfCanvas BuildSprite()
{
    static const Vec2f s_largeMountain[] = { Vec2f(-0.56f, -0.44f), Vec2f(-0.12f, 0.16f), Vec2f(0.32f, -0.44f) };
    static const Vec2f s_smallMountain[] = { Vec2f(0.06f, -0.44f), Vec2f(0.32f, -0.08f), Vec2f(0.58f, -0.44f) };

    SdfCanvas canvas;
    AddPictureFrame(canvas)
        .ConvexPolygon(s_largeMountain)
        .ConvexPolygon(s_smallMountain)
        .Circle(Vec2f(0.36f, 0.28f), 0.13f);

    return canvas;
}

static SdfCanvas BuildTextSprite()
{
    SdfCanvas canvas;
    AddPictureFrame(canvas)
        .Segment(Vec2f(-0.38f, 0.3f), Vec2f(0.38f, 0.3f), 0.09f)
        .Segment(Vec2f(0.0f, 0.3f), Vec2f(0.0f, -0.36f), 0.09f);

    return canvas;
}

static SdfCanvas BuildParticleVolume()
{
    static const Vec2f s_sparkleCenter = Vec2f(-0.14f, 0.1f);

    static const Vec2f s_verticalRay[] = {
        s_sparkleCenter + Vec2f(0.0f, 0.62f),
        s_sparkleCenter + Vec2f(0.15f, 0.0f),
        s_sparkleCenter + Vec2f(0.0f, -0.62f),
        s_sparkleCenter + Vec2f(-0.15f, 0.0f)
    };

    static const Vec2f s_horizontalRay[] = {
        s_sparkleCenter + Vec2f(0.62f, 0.0f),
        s_sparkleCenter + Vec2f(0.0f, -0.15f),
        s_sparkleCenter + Vec2f(-0.62f, 0.0f),
        s_sparkleCenter + Vec2f(0.0f, 0.15f)
    };

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_verticalRay)
        .ConvexPolygon(s_horizontalRay)
        .Circle(Vec2f(0.56f, 0.56f), 0.13f)
        .Circle(Vec2f(0.58f, -0.44f), 0.17f)
        .Circle(Vec2f(-0.6f, -0.62f), 0.1f);

    return canvas;
}

static SdfCanvas BuildFogVolume()
{
    SdfCanvas canvas;
    canvas.Circle(Vec2f(-0.36f, 0.08f), 0.28f)
        .Circle(Vec2f(0.08f, 0.26f), 0.38f)
        .Circle(Vec2f(0.5f, 0.06f), 0.26f)
        .RoundedBox(Vec2f(0.06f, -0.08f), Vec2f(0.7f, 0.2f), 0.2f)
        .Segment(Vec2f(-0.62f, -0.5f), Vec2f(0.38f, -0.5f), 0.065f)
        .Segment(Vec2f(-0.3f, -0.72f), Vec2f(0.7f, -0.72f), 0.065f);

    return canvas;
}

static SdfCanvas BuildDecal()
{
    struct SplatArm
    {
        Vec2f lobeCenter;
        float lobeRadius;
        float neckRadius;
    };

    // arms neck in from the body and swell into a round lobe
    static const SplatArm s_arms[] = {
        { Vec2f(0.36f, 0.72f), 0.16f, 0.05f },
        { Vec2f(-0.05f, 0.58f), 0.1f, 0.06f },
        { Vec2f(-0.5f, 0.48f), 0.14f, 0.055f },
        { Vec2f(0.6f, 0.2f), 0.08f, 0.05f },
        { Vec2f(-0.74f, -0.08f), 0.17f, 0.06f },
        { Vec2f(0.72f, -0.34f), 0.16f, 0.055f },
        { Vec2f(0.28f, -0.46f), 0.08f, 0.05f },
        { Vec2f(-0.4f, -0.44f), 0.09f, 0.05f },
        { Vec2f(-0.04f, -0.76f), 0.15f, 0.055f }
    };

    static constexpr float s_bodyRadius = 0.36f;
    static constexpr float s_neckStartDistance = 0.28f;

    SdfCanvas canvas;
    canvas.Blend(0.15f)
        .Circle(Vec2f(0.0f, 0.02f), s_bodyRadius);

    for (const SplatArm& arm : s_arms)
    {
        const Vec2f direction = arm.lobeCenter / arm.lobeCenter.Length();

        canvas.Segment(direction * s_neckStartDistance, arm.lobeCenter, arm.neckRadius)
            .Circle(arm.lobeCenter, arm.lobeRadius);
    }

    return canvas;
}

static const Map<Icon, SdfCanvas (*)(void)> s_builders = {
    { Icon::PointLight, &BuildPointLight },
    { Icon::SpotLight, &BuildSpotLight },
    { Icon::DirectionalLight, &BuildDirectionalLight },
    { Icon::AreaLight, &BuildAreaLight },
    { Icon::Camera, &BuildCamera },
    { Icon::EnvProbe, &BuildEnvProbe },
    { Icon::LightmapVolume, &BuildLightmapVolume },
    { Icon::Node, &BuildNode },
    { Icon::Entity, &BuildEntity },
    { Icon::Sprite, &BuildSprite },
    { Icon::TextSprite, &BuildTextSprite },
    { Icon::ParticleVolume, &BuildParticleVolume },
    { Icon::FogVolume, &BuildFogVolume },
    { Icon::Decal, &BuildDecal }
};

SdfCanvas BuildCanvas(Icon icon)
{
    const auto it = s_builders.Find(icon);

    if (it == s_builders.End())
    {
        return {};
    }

    return it->second();
}

const SdfRasterStyle& GetSpriteRasterStyle()
{
    static const SdfRasterStyle s_style {
        /* outlineWidth */ 0.09f,
        /* outlineOpacity */ 0.85f,
        /* outlineShade */ 0.08f
    };

    return s_style;
}

const char* GetFileStem(Icon icon)
{
    switch (icon)
    {
    case Icon::PointLight:
        return "point-light";
    case Icon::SpotLight:
        return "spot-light";
    case Icon::DirectionalLight:
        return "directional-light";
    case Icon::AreaLight:
        return "area-light";
    case Icon::Camera:
        return "camera";
    case Icon::EnvProbe:
        return "env-probe";
    case Icon::LightmapVolume:
        return "lightmap-volume";
    case Icon::Node:
        return "node";
    case Icon::Entity:
        return "entity";
    case Icon::Sprite:
        return "sprite";
    case Icon::TextSprite:
        return "text-sprite";
    case Icon::ParticleVolume:
        return "particle-volume";
    case Icon::FogVolume:
        return "fog-volume";
    case Icon::Decal:
        return "decal";
    default:
        return "";
    }
}

Name GetSpriteTextureName(Icon icon)
{
    return NAME_FMT("EditorIcon_{}", GetFileStem(icon));
}

Handle<Texture> CreateSpriteTexture(Icon icon, uint32 spriteTextureSize)
{
    ByteBuffer imageBytes(BuildCanvas(icon).Rasterize(spriteTextureSize, GetSpriteRasterStyle()).ToByteView());

    TextureDesc textureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8_SRGB,
        Vec3u { spriteTextureSize, spriteTextureSize, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    };

    Texture::GenerateMipmaps(textureDesc, imageBytes);

    Handle<Texture> texture = MakeHandle<Texture>(textureDesc, imageBytes.ToByteView());
    texture->SetName(GetSpriteTextureName(icon));

    return texture;
}

} // namespace EditorIcons

} // namespace Hyperion

#endif // HYP_EDITOR
