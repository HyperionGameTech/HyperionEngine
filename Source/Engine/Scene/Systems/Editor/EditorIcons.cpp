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

static Vec2f HexagonVertex(float radius, int vertexIndex)
{
    // pointy-top, counter-clockwise from the top vertex
    const float angle = MathUtil::pi<float> * (0.5f + float(vertexIndex) / 3.0f);

    return Vec2f(std::cos(angle), std::sin(angle)) * radius;
}

struct IsoCube
{
    Vec2f topFace[4];
    Vec2f leftFace[4];
    Vec2f rightFace[4];
    Vec2f outline[6];
};

static IsoCube MakeIsoCube(const Vec2f& center, float radius)
{
    const Vec2f top = center + Vec2f(0.0f, radius);
    const Vec2f upperRight = center + Vec2f(0.866f * radius, 0.5f * radius);
    const Vec2f lowerRight = center + Vec2f(0.866f * radius, -0.5f * radius);
    const Vec2f bottom = center + Vec2f(0.0f, -radius);
    const Vec2f lowerLeft = center + Vec2f(-0.866f * radius, -0.5f * radius);
    const Vec2f upperLeft = center + Vec2f(-0.866f * radius, 0.5f * radius);

    return IsoCube {
        { top, upperRight, center, upperLeft },
        { upperLeft, center, bottom, lowerLeft },
        { center, upperRight, lowerRight, bottom },
        { top, upperRight, lowerRight, bottom, lowerLeft, upperLeft }
    };
}

static SdfCanvas& AddIsoCubeFaces(SdfCanvas& canvas, const IsoCube& cube, float seamInset)
{
    return canvas.ConvexPolygon(cube.topFace, seamInset)
        .ConvexPolygon(cube.leftFace, seamInset)
        .ConvexPolygon(cube.rightFace, seamInset);
}

static SdfCanvas BuildAssetMesh()
{
    static constexpr float s_meshRadius = 0.74f;
    static constexpr float s_edgeRadius = 0.06f;
    static constexpr float s_vertexRadius = 0.14f;

    SdfCanvas canvas;
    canvas.Circle(Vec2f::Zero(), s_vertexRadius);

    for (int vertexIndex = 0; vertexIndex < 6; vertexIndex++)
    {
        const Vec2f vertex = HexagonVertex(s_meshRadius, vertexIndex);
        const Vec2f nextVertex = HexagonVertex(s_meshRadius, (vertexIndex + 1) % 6);

        canvas.Segment(vertex, nextVertex, s_edgeRadius)
            .Segment(Vec2f::Zero(), vertex, s_edgeRadius)
            .Circle(vertex, s_vertexRadius);
    }

    return canvas;
}

// a scalpel over a dashed cut line
static SdfCanvas BuildMeshEditMode()
{
    // laid out along +x with the blade tip at +x, then rotated onto the diagonal
    static const Vec2f s_blade[] = {
        Vec2f(0.16f, 0.07f),
        Vec2f(0.16f, -0.1f),
        Vec2f(0.44f, -0.17f),
        Vec2f(0.68f, -0.14f),
        Vec2f(0.86f, -0.05f),
        Vec2f(0.98f, 0.08f),
        Vec2f(0.6f, 0.08f)
    };

    static const Vec2f s_offset = Vec2f(-0.1f, 0.12f);

    static constexpr float s_scale = 0.92f;

    const float angle = MathUtil::DegToRad(40.0f);
    const float cosAngle = std::cos(angle);
    const float sinAngle = std::sin(angle);

    const auto place = [&](const Vec2f& local) -> Vec2f
    {
        const Vec2f scaled = local * s_scale;

        return s_offset + Vec2f(scaled.x * cosAngle - scaled.y * sinAngle, scaled.x * sinAngle + scaled.y * cosAngle);
    };

    Vec2f bladeCorners[GetArrayCount(s_blade)];

    for (uint32 cornerIndex = 0; cornerIndex < uint32(GetArrayCount(s_blade)); cornerIndex++)
    {
        bladeCorners[cornerIndex] = place(s_blade[cornerIndex]);
    }

    SdfCanvas canvas;
    canvas.Segment(place(Vec2f(-0.96f, 0.0f)), place(Vec2f(0.04f, 0.0f)), 0.085f * s_scale)
        .Segment(place(Vec2f(0.04f, 0.0f)), place(Vec2f(0.18f, -0.01f)), 0.055f * s_scale)
        .ConvexPolygon(bladeCorners)
        .Subtract()
        .Segment(place(Vec2f(0.12f, 0.2f)), place(Vec2f(0.12f, -0.2f)), 0.03f * s_scale);

    for (float gripNotchX : { -0.62f, -0.5f, -0.38f })
    {
        canvas.Segment(place(Vec2f(gripNotchX, 0.2f)), place(Vec2f(gripNotchX, -0.2f)), 0.022f * s_scale);
    }

    canvas.Union();

    for (float dashStartX : { -0.9f, -0.5f, -0.1f, 0.3f })
    {
        canvas.Segment(Vec2f(dashStartX, -0.78f), Vec2f(dashStartX + 0.22f, -0.78f), 0.05f);
    }

    return canvas;
}

static SdfCanvas BuildAssetTexture()
{
    static constexpr float s_cellSize = 0.34f;

    SdfCanvas canvas;
    canvas.RoundedBox(Vec2f::Zero(), Vec2f(0.82f, 0.82f), 0.16f)
        .Subtract()
        .RoundedBox(Vec2f::Zero(), Vec2f(0.68f, 0.68f), 0.06f)
        .Union();

    for (int row = 0; row < 4; row++)
    {
        for (int column = 0; column < 4; column++)
        {
            if ((row + column) % 2 != 0)
            {
                continue;
            }

            const Vec2f cellCenter = Vec2f(-0.51f + float(column) * s_cellSize, 0.51f - float(row) * s_cellSize);

            canvas.RoundedBox(cellCenter, Vec2f(s_cellSize * 0.5f, s_cellSize * 0.5f), 0.0f);
        }
    }

    return canvas;
}

// a tuxedo - what the mesh wears
static SdfCanvas BuildAssetMaterial()
{
    static const Vec2f s_shirt[] = { Vec2f(-0.3f, 0.86f), Vec2f(0.3f, 0.86f), Vec2f(0.0f, -0.84f) };

    static const Vec2f s_bowTieCenter = Vec2f(0.0f, 0.56f);
    static const Vec2f s_bowTieLeft[] = { Vec2f(-0.4f, 0.78f), Vec2f(-0.4f, 0.34f), s_bowTieCenter };
    static const Vec2f s_bowTieRight[] = { Vec2f(0.4f, 0.78f), Vec2f(0.4f, 0.34f), s_bowTieCenter };

    static constexpr float s_strokeRadius = 0.07f;
    static constexpr float s_bowTieGap = 0.08f;
    static constexpr float s_knotRadius = 0.1f;

    SdfCanvas canvas;

    for (int cornerIndex = 0; cornerIndex < 3; cornerIndex++)
    {
        canvas.Segment(s_shirt[cornerIndex], s_shirt[(cornerIndex + 1) % 3], s_strokeRadius);
    }

    // shoulders and sides of the jacket
    for (float side : { -1.0f, 1.0f })
    {
        canvas.Segment(Vec2f(side * 0.3f, 0.86f), Vec2f(side * 0.84f, 0.54f), s_strokeRadius)
            .Segment(Vec2f(side * 0.84f, 0.54f), Vec2f(side * 0.84f, -0.84f), s_strokeRadius);
    }

    canvas.Subtract()
        .ConvexPolygon(s_bowTieLeft, -s_bowTieGap)
        .ConvexPolygon(s_bowTieRight, -s_bowTieGap)
        .Circle(s_bowTieCenter, s_knotRadius + s_bowTieGap)
        .Union()
        .ConvexPolygon(s_bowTieLeft)
        .ConvexPolygon(s_bowTieRight)
        .Circle(s_bowTieCenter, s_knotRadius)
        .Circle(Vec2f(0.0f, 0.14f), 0.06f)
        .Circle(Vec2f(0.0f, -0.1f), 0.06f);

    return canvas;
}

static SdfCanvas BuildAssetInstancedMesh()
{
    static constexpr float s_seamInset = 0.03f;

    const IsoCube frontCube = MakeIsoCube(Vec2f(0.0f, 0.3f), 0.42f);

    SdfCanvas canvas;
    AddIsoCubeFaces(canvas, MakeIsoCube(Vec2f(-0.4f, -0.4f), 0.42f), s_seamInset);
    AddIsoCubeFaces(canvas, MakeIsoCube(Vec2f(0.4f, -0.4f), 0.42f), s_seamInset);

    canvas.Subtract()
        .ConvexPolygon(frontCube.outline, -0.08f)
        .Union();

    AddIsoCubeFaces(canvas, frontCube, s_seamInset);

    return canvas;
}

static SdfCanvas BuildAssetAnimation()
{
    static const Vec2f s_playTriangle[] = { Vec2f(-0.16f, 0.24f), Vec2f(0.26f, 0.0f), Vec2f(-0.16f, -0.24f) };

    SdfCanvas canvas;
    canvas.RoundedBox(Vec2f::Zero(), Vec2f(0.86f, 0.66f), 0.1f)
        .Subtract();

    for (float perforationX : { -0.6f, -0.2f, 0.2f, 0.6f })
    {
        canvas.RoundedBox(Vec2f(perforationX, 0.5f), Vec2f(0.09f, 0.07f), 0.02f)
            .RoundedBox(Vec2f(perforationX, -0.5f), Vec2f(0.09f, 0.07f), 0.02f);
    }

    canvas.ConvexPolygon(s_playTriangle);

    return canvas;
}

static SdfCanvas BuildAssetAnimationTrack()
{
    static const Vec2f s_keyframeCenters[] = { Vec2f(-0.4f, 0.38f), Vec2f(0.44f, 0.38f), Vec2f(0.0f, -0.38f) };

    static constexpr float s_keyframeRadius = 0.24f;

    SdfCanvas canvas;
    canvas.Segment(Vec2f(-0.86f, 0.38f), Vec2f(0.86f, 0.38f), 0.055f)
        .Segment(Vec2f(-0.86f, -0.38f), Vec2f(0.86f, -0.38f), 0.055f);

    for (const Vec2f& center : s_keyframeCenters)
    {
        const Vec2f keyframe[] = {
            center + Vec2f(0.0f, s_keyframeRadius),
            center + Vec2f(s_keyframeRadius, 0.0f),
            center + Vec2f(0.0f, -s_keyframeRadius),
            center + Vec2f(-s_keyframeRadius, 0.0f)
        };

        canvas.ConvexPolygon(keyframe);
    }

    return canvas;
}

static SdfCanvas BuildAssetSkeleton()
{
    SdfCanvas canvas;
    canvas.Blend(0.12f)
        .Segment(Vec2f(-0.44f, -0.44f), Vec2f(0.44f, 0.44f), 0.11f)
        .Circle(Vec2f(0.42f, 0.66f), 0.19f)
        .Circle(Vec2f(0.66f, 0.42f), 0.19f)
        .Circle(Vec2f(-0.66f, -0.42f), 0.19f)
        .Circle(Vec2f(-0.42f, -0.66f), 0.19f);

    return canvas;
}

static SdfCanvas BuildAssetWorld()
{
    // a desk globe
    static const Vec2f s_globeCenter = Vec2f(0.0f, 0.14f);

    static constexpr float s_globeRadius = 0.6f;
    static constexpr float s_lineRadius = 0.045f;

    static constexpr float s_standRadius = 0.78f;
    static constexpr int s_numStandSegments = 10;

    SdfCanvas canvas;
    canvas.Circle(s_globeCenter, s_globeRadius)
        .Subtract()
        .EllipseRing(s_globeCenter, Vec2f(0.4f, s_globeRadius), s_lineRadius)
        .Segment(Vec2f(0.0f, 0.8f), Vec2f(0.0f, -0.5f), s_lineRadius);

    for (float latitudeOffset : { 0.0f, 0.3f, -0.3f })
    {
        const float latitudeY = s_globeCenter.y + latitudeOffset;

        canvas.Segment(Vec2f(-0.7f, latitudeY), Vec2f(0.7f, latitudeY), s_lineRadius);
    }

    canvas.Union();

    // stand arc cradles the lower part of the globe
    for (int segmentIndex = 0; segmentIndex < s_numStandSegments; segmentIndex++)
    {
        const float startAngle = MathUtil::DegToRad(200.0f + 140.0f * float(segmentIndex) / float(s_numStandSegments));
        const float endAngle = MathUtil::DegToRad(200.0f + 140.0f * float(segmentIndex + 1) / float(s_numStandSegments));

        canvas.Segment(
            s_globeCenter + Vec2f(std::cos(startAngle), std::sin(startAngle)) * s_standRadius,
            s_globeCenter + Vec2f(std::cos(endAngle), std::sin(endAngle)) * s_standRadius,
            0.065f);
    }

    canvas.Segment(Vec2f(0.0f, -0.64f), Vec2f(0.0f, -0.8f), 0.07f)
        .RoundedBox(Vec2f(0.0f, -0.84f), Vec2f(0.36f, 0.07f), 0.07f);

    return canvas;
}

static SdfCanvas BuildAssetScene()
{
    static const Vec2f s_ground[] = { Vec2f(-0.88f, -0.42f), Vec2f(0.0f, -0.84f), Vec2f(0.88f, -0.42f), Vec2f(0.0f, 0.0f) };

    static const Vec2f s_sphereCenter = Vec2f(0.4f, 0.22f);

    const IsoCube cube = MakeIsoCube(Vec2f(-0.3f, 0.12f), 0.34f);

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_ground)
        .Subtract()
        .ConvexPolygon(cube.outline, -0.08f)
        .Circle(s_sphereCenter, 0.34f)
        .Union();

    AddIsoCubeFaces(canvas, cube, 0.025f)
        .Circle(s_sphereCenter, 0.26f);

    return canvas;
}

static SdfCanvas& AddPrismOutline(SdfCanvas& canvas, Span<const Vec2f> corners)
{
    for (uint32 cornerIndex = 0; cornerIndex < 3; cornerIndex++)
    {
        canvas.Segment(corners[cornerIndex], corners[(cornerIndex + 1) % 3], 0.07f);
    }

    return canvas;
}

// a white beam entering a prism, split into rays leaving it
static SdfCanvas& AddLightBeam(SdfCanvas& canvas, const Vec2f& beamStart, const Vec2f& beamEnd, const Vec2f& raysOrigin, Span<const Vec2f> rayEnds)
{
    static constexpr float s_beamRadius = 0.055f;

    canvas.Segment(beamStart, beamEnd, s_beamRadius);

    for (const Vec2f& rayEnd : rayEnds)
    {
        canvas.Segment(raysOrigin, rayEnd, s_beamRadius);
    }

    return canvas;
}

static SdfCanvas BuildAssetShader()
{
    static const Vec2f s_prism[] = { Vec2f(0.0f, 0.62f), Vec2f(0.52f, -0.3f), Vec2f(-0.52f, -0.3f) };
    static const Vec2f s_rayEnds[] = { Vec2f(0.95f, 0.3f), Vec2f(0.95f, 0.0f), Vec2f(0.95f, -0.3f) };

    SdfCanvas canvas;
    AddPrismOutline(canvas, s_prism);
    AddLightBeam(canvas, Vec2f(-0.95f, 0.05f), Vec2f(-0.28f, 0.2f), Vec2f(0.3f, 0.12f), s_rayEnds);

    return canvas;
}

static SdfCanvas BuildAssetShaderBundle()
{
    static const Vec2f s_backPrism[] = { Vec2f(0.26f, 0.9f), Vec2f(0.7f, 0.14f), Vec2f(-0.18f, 0.14f) };
    static const Vec2f s_frontPrism[] = { Vec2f(-0.06f, 0.54f), Vec2f(0.5f, -0.42f), Vec2f(-0.62f, -0.42f) };
    static const Vec2f s_rayEnds[] = { Vec2f(0.92f, 0.0f), Vec2f(0.92f, -0.26f), Vec2f(0.92f, -0.52f) };

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_backPrism)
        .Subtract()
        .ConvexPolygon(s_frontPrism, -0.12f)
        .Union();

    AddPrismOutline(canvas, s_frontPrism);
    AddLightBeam(canvas, Vec2f(-0.98f, -0.1f), Vec2f(-0.36f, 0.04f), Vec2f(0.2f, -0.04f), s_rayEnds);

    return canvas;
}

static SdfCanvas BuildAssetFontAtlas()
{
    static const Vec2f s_thickStroke[] = { Vec2f(-0.1f, 0.8f), Vec2f(0.1f, 0.8f), Vec2f(0.6f, -0.62f), Vec2f(0.32f, -0.62f) };

    static constexpr float s_thinRadius = 0.05f;

    SdfCanvas canvas;
    canvas.Segment(Vec2f(-0.5f, -0.62f), Vec2f(-0.04f, 0.74f), s_thinRadius)
        .ConvexPolygon(s_thickStroke)
        .Segment(Vec2f(-0.32f, -0.1f), Vec2f(0.38f, -0.1f), 0.045f)
        .Segment(Vec2f(-0.72f, -0.66f), Vec2f(-0.3f, -0.66f), s_thinRadius)
        .Segment(Vec2f(0.2f, -0.66f), Vec2f(0.76f, -0.66f), s_thinRadius);

    return canvas;
}

static SdfCanvas BuildAssetPhysicsShape()
{
    // a falling apple
    static const Vec2f s_leaf[] = { Vec2f(0.1f, 0.16f), Vec2f(0.3f, 0.3f), Vec2f(0.48f, 0.26f), Vec2f(0.3f, 0.12f) };

    static const Vec2f s_fallLines[][2] = {
        { Vec2f(-0.36f, 0.9f), Vec2f(-0.36f, 0.54f) },
        { Vec2f(0.0f, 0.94f), Vec2f(0.0f, 0.58f) },
        { Vec2f(0.36f, 0.9f), Vec2f(0.36f, 0.54f) }
    };

    SdfCanvas canvas;
    canvas.Blend(0.18f)
        .Circle(Vec2f(-0.17f, -0.3f), 0.4f)
        .Circle(Vec2f(0.17f, -0.3f), 0.4f)
        .Blend(0.0f)
        .Subtract()
        .Circle(Vec2f(0.0f, 0.14f), 0.1f)
        .Union()
        .Segment(Vec2f(0.0f, 0.0f), Vec2f(0.06f, 0.26f), 0.05f)
        .ConvexPolygon(s_leaf);

    for (const auto& fallLine : s_fallLines)
    {
        canvas.Segment(fallLine[0], fallLine[1], 0.05f);
    }

    return canvas;
}

static SdfCanvas BuildAssetScript()
{
    // right-hand brace, mirrored for the left
    static const Vec2f s_brace[] = {
        Vec2f(0.24f, 0.76f),
        Vec2f(0.46f, 0.64f),
        Vec2f(0.46f, 0.14f),
        Vec2f(0.72f, 0.0f),
        Vec2f(0.46f, -0.14f),
        Vec2f(0.46f, -0.64f),
        Vec2f(0.24f, -0.76f)
    };

    SdfCanvas canvas;

    for (float side : { -1.0f, 1.0f })
    {
        for (uint32 pointIndex = 0; pointIndex + 1 < uint32(GetArrayCount(s_brace)); pointIndex++)
        {
            const Vec2f& start = s_brace[pointIndex];
            const Vec2f& end = s_brace[pointIndex + 1];

            canvas.Segment(Vec2f(start.x * side, start.y), Vec2f(end.x * side, end.y), 0.085f);
        }
    }

    return canvas;
}

static SdfCanvas BuildAssetRawData()
{
    static const Vec2f s_page[] = { Vec2f(-0.66f, -0.86f), Vec2f(0.66f, -0.86f), Vec2f(0.66f, 0.46f), Vec2f(0.26f, 0.86f), Vec2f(-0.66f, 0.86f) };

    static const Vec2f s_zeroRadii = Vec2f(0.14f, 0.24f);

    static constexpr float s_digitRadius = 0.055f;

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_page)
        .Subtract()
        .EllipseRing(Vec2f(-0.24f, 0.26f), s_zeroRadii, s_digitRadius)
        .Segment(Vec2f(0.24f, 0.5f), Vec2f(0.24f, 0.02f), s_digitRadius)
        .Segment(Vec2f(-0.24f, -0.14f), Vec2f(-0.24f, -0.62f), s_digitRadius)
        .EllipseRing(Vec2f(0.24f, -0.38f), s_zeroRadii, s_digitRadius);

    return canvas;
}

// a toy brick seen from above, 2x2 studs on its top face
static SdfCanvas BuildAssetPrefab()
{
    static const Vec2f s_topFace[] = { Vec2f(0.0f, 0.62f), Vec2f(0.86f, 0.2f), Vec2f(0.0f, -0.22f), Vec2f(-0.86f, 0.2f) };
    static const Vec2f s_leftFace[] = { Vec2f(-0.86f, 0.2f), Vec2f(0.0f, -0.22f), Vec2f(0.0f, -0.82f), Vec2f(-0.86f, -0.4f) };
    static const Vec2f s_rightFace[] = { Vec2f(0.0f, -0.22f), Vec2f(0.86f, 0.2f), Vec2f(0.86f, -0.4f), Vec2f(0.0f, -0.82f) };

    static const Vec2f s_studCenters[] = { Vec2f(0.0f, 0.42f), Vec2f(-0.42f, 0.2f), Vec2f(0.42f, 0.2f), Vec2f(0.0f, -0.02f) };

    static constexpr float s_seamInset = 0.035f;

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_topFace, s_seamInset)
        .ConvexPolygon(s_leftFace, s_seamInset)
        .ConvexPolygon(s_rightFace, s_seamInset)
        .Subtract();

    for (const Vec2f& studCenter : s_studCenters)
    {
        canvas.RoundedBox(studCenter + Vec2f(0.0f, 0.06f), Vec2f(0.17f, 0.12f), 0.1f);
    }

    canvas.Union();

    for (const Vec2f& studCenter : s_studCenters)
    {
        canvas.RoundedBox(studCenter + Vec2f(0.0f, 0.08f), Vec2f(0.12f, 0.08f), 0.06f);
    }

    return canvas;
}

static SdfCanvas BuildAssetSound()
{
    static const Vec2f s_waveOrigin = Vec2f(-0.2f, 0.0f);

    // everything outside the wedge the waves spread through
    static const Vec2f s_aboveWaves[] = { s_waveOrigin, Vec2f(0.8f, 1.2f), Vec2f(-1.2f, 1.2f), Vec2f(-1.2f, 0.0f) };
    static const Vec2f s_belowWaves[] = { s_waveOrigin, Vec2f(-1.2f, 0.0f), Vec2f(-1.2f, -1.2f), Vec2f(0.8f, -1.2f) };

    static const Vec2f s_cone[] = { Vec2f(-0.6f, 0.2f), Vec2f(-0.26f, 0.52f), Vec2f(-0.26f, -0.52f), Vec2f(-0.6f, -0.2f) };

    SdfCanvas canvas;
    canvas.Ring(s_waveOrigin, 0.42f, 0.065f)
        .Ring(s_waveOrigin, 0.72f, 0.065f)
        .Subtract()
        .ConvexPolygon(s_aboveWaves)
        .ConvexPolygon(s_belowWaves)
        .Union()
        .RoundedBox(Vec2f(-0.7f, 0.0f), Vec2f(0.16f, 0.2f), 0.04f)
        .ConvexPolygon(s_cone);

    return canvas;
}

static SdfCanvas BuildAssetTerrain()
{
    static const Vec2f s_farPeak[] = { Vec2f(0.02f, -0.56f), Vec2f(0.5f, 0.12f), Vec2f(0.96f, -0.56f) };
    static const Vec2f s_nearPeak[] = { Vec2f(-0.9f, -0.56f), Vec2f(-0.18f, 0.62f), Vec2f(0.54f, -0.56f) };

    static const Vec2f s_snowLine[] = { Vec2f(-0.52f, 0.06f), Vec2f(-0.36f, -0.06f), Vec2f(-0.2f, 0.08f), Vec2f(-0.02f, -0.06f), Vec2f(0.14f, 0.06f) };

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_farPeak)
        .Subtract()
        .ConvexPolygon(s_nearPeak, -0.08f)
        .Union()
        .ConvexPolygon(s_nearPeak)
        .Subtract();

    for (uint32 pointIndex = 0; pointIndex + 1 < uint32(GetArrayCount(s_snowLine)); pointIndex++)
    {
        canvas.Segment(s_snowLine[pointIndex], s_snowLine[pointIndex + 1], 0.05f);
    }

    canvas.Union()
        .Segment(Vec2f(-0.9f, -0.78f), Vec2f(0.96f, -0.78f), 0.065f);

    return canvas;
}

static SdfCanvas BuildAssetWeapon()
{
    static const Vec2f s_blade[] = { Vec2f(-0.285f, -0.115f), Vec2f(-0.115f, -0.285f), Vec2f(0.62f, 0.45f), Vec2f(0.82f, 0.82f), Vec2f(0.45f, 0.62f) };

    SdfCanvas canvas;
    canvas.ConvexPolygon(s_blade)
        .Segment(Vec2f(-0.53f, -0.03f), Vec2f(-0.03f, -0.53f), 0.075f)
        .Segment(Vec2f(-0.32f, -0.32f), Vec2f(-0.6f, -0.6f), 0.07f)
        .Circle(Vec2f(-0.7f, -0.7f), 0.12f);

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
    { Icon::Decal, &BuildDecal },
    { Icon::AssetMesh, &BuildAssetMesh },
    { Icon::AssetTexture, &BuildAssetTexture },
    { Icon::AssetMaterial, &BuildAssetMaterial },
    { Icon::AssetInstancedMesh, &BuildAssetInstancedMesh },
    { Icon::AssetAnimation, &BuildAssetAnimation },
    { Icon::AssetAnimationTrack, &BuildAssetAnimationTrack },
    { Icon::AssetSkeleton, &BuildAssetSkeleton },
    { Icon::AssetWorld, &BuildAssetWorld },
    { Icon::AssetScene, &BuildAssetScene },
    { Icon::AssetShader, &BuildAssetShader },
    { Icon::AssetShaderBundle, &BuildAssetShaderBundle },
    { Icon::AssetFontAtlas, &BuildAssetFontAtlas },
    { Icon::AssetPhysicsShape, &BuildAssetPhysicsShape },
    { Icon::AssetScript, &BuildAssetScript },
    { Icon::AssetRawData, &BuildAssetRawData },
    { Icon::AssetPrefab, &BuildAssetPrefab },
    { Icon::AssetSound, &BuildAssetSound },
    { Icon::AssetTerrain, &BuildAssetTerrain },
    { Icon::AssetWeapon, &BuildAssetWeapon },
    { Icon::MeshEditMode, &BuildMeshEditMode }
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
    case Icon::AssetMesh:
        return "asset-mesh";
    case Icon::AssetTexture:
        return "asset-texture";
    case Icon::AssetMaterial:
        return "asset-material";
    case Icon::AssetInstancedMesh:
        return "asset-instanced-mesh";
    case Icon::AssetAnimation:
        return "asset-animation";
    case Icon::AssetAnimationTrack:
        return "asset-animation-track";
    case Icon::AssetSkeleton:
        return "asset-skeleton";
    case Icon::AssetWorld:
        return "asset-world";
    case Icon::AssetScene:
        return "asset-scene";
    case Icon::AssetShader:
        return "asset-shader";
    case Icon::AssetShaderBundle:
        return "asset-shader-bundle";
    case Icon::AssetFontAtlas:
        return "asset-font-atlas";
    case Icon::AssetPhysicsShape:
        return "asset-physics-shape";
    case Icon::AssetScript:
        return "asset-script";
    case Icon::AssetRawData:
        return "asset-raw-data";
    case Icon::AssetPrefab:
        return "asset-prefab";
    case Icon::AssetSound:
        return "asset-sound";
    case Icon::AssetTerrain:
        return "asset-terrain";
    case Icon::AssetWeapon:
        return "asset-weapon";
    case Icon::MeshEditMode:
        return "mesh-edit";
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
