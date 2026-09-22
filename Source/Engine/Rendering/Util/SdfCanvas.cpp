/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/Util/SdfCanvas.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

namespace Hyperion {

static float SdCircle(const Vec2f& point, const Vec2f& center, float radius)
{
    return (point - center).Length() - radius;
}

static float SdEllipseRing(const Vec2f& point, const Vec2f& center, const Vec2f& radii, float halfWidth)
{
    const Vec2f local = point - center;
    const Vec2f scaled = Vec2f(local.x / radii.x, local.y / radii.y);
    const float scaledLength = scaled.Length();

    if (scaledLength < 1e-5f)
    {
        return MathUtil::Min(radii.x, radii.y) - halfWidth;
    }

    const Vec2f gradient = Vec2f(local.x / (radii.x * radii.x), local.y / (radii.y * radii.y));
    const float gradientLength = gradient.Length() / scaledLength;

    return MathUtil::Abs(scaledLength - 1.0f) / gradientLength - halfWidth;
}

static float SdRoundedBox(const Vec2f& point, const Vec2f& center, const Vec2f& halfExtents, float cornerRadius)
{
    const float qx = MathUtil::Abs(point.x - center.x) - halfExtents.x + cornerRadius;
    const float qy = MathUtil::Abs(point.y - center.y) - halfExtents.y + cornerRadius;

    const Vec2f outside = Vec2f(MathUtil::Max(qx, 0.0f), MathUtil::Max(qy, 0.0f));

    return outside.Length() + MathUtil::Min(MathUtil::Max(qx, qy), 0.0f) - cornerRadius;
}

static float SdSegment(const Vec2f& point, const Vec2f& start, const Vec2f& end, float radius)
{
    const Vec2f toPoint = point - start;
    const Vec2f segment = end - start;

    const float t = MathUtil::Clamp(toPoint.Dot(segment) / segment.Dot(segment), 0.0f, 1.0f);

    return (toPoint - segment * t).Length() - radius;
}

void SdfCanvas::AddPrimitive(Primitive primitive)
{
    primitive.blend = m_blend;

    m_primitives.PushBack(primitive);
}

SdfCanvas& SdfCanvas::Blend(float radius)
{
    m_blend = MathUtil::Max(radius, 0.0f);

    return *this;
}

SdfCanvas& SdfCanvas::Circle(const Vec2f& center, float radius)
{
    AddPrimitive(Primitive { PrimitiveType::Circle, center, Vec2f::Zero(), radius });

    return *this;
}

SdfCanvas& SdfCanvas::Ring(const Vec2f& center, float radius, float halfWidth)
{
    AddPrimitive(Primitive { PrimitiveType::Ring, center, Vec2f::Zero(), radius, halfWidth });

    return *this;
}

SdfCanvas& SdfCanvas::EllipseRing(const Vec2f& center, const Vec2f& radii, float halfWidth)
{
    AddPrimitive(Primitive { PrimitiveType::EllipseRing, center, radii, 0.0f, halfWidth });

    return *this;
}

SdfCanvas& SdfCanvas::RoundedBox(const Vec2f& center, const Vec2f& halfExtents, float cornerRadius)
{
    AddPrimitive(Primitive { PrimitiveType::RoundedBox, center, halfExtents, cornerRadius });

    return *this;
}

SdfCanvas& SdfCanvas::Segment(const Vec2f& start, const Vec2f& end, float radius)
{
    AddPrimitive(Primitive { PrimitiveType::Segment, start, end, radius });

    return *this;
}

SdfCanvas& SdfCanvas::ConvexPolygon(Span<const Vec2f> points, float inset)
{
    if (points.Size() < 3)
    {
        return *this;
    }

    float signedArea = 0.0f;

    for (size_t i = 0; i < points.Size(); i++)
    {
        const Vec2f& a = points[i];
        const Vec2f& b = points[(i + 1) % points.Size()];

        signedArea += a.x * b.y - b.x * a.y;
    }

    const float windingSign = signedArea > 0.0f ? 1.0f : -1.0f;

    Primitive primitive { PrimitiveType::ConvexPolygon };
    primitive.halfWidth = inset;
    primitive.firstEdge = uint32(m_polygonEdges.Size());
    primitive.numEdges = uint32(points.Size());

    for (size_t i = 0; i < points.Size(); i++)
    {
        const Vec2f& a = points[i];
        const Vec2f& b = points[(i + 1) % points.Size()];

        const Vec2f edge = b - a;
        const float edgeLength = edge.Length();

        m_polygonEdges.PushBack(PolygonEdge { a, Vec2f(edge.y, -edge.x) * (windingSign / edgeLength) });
    }

    AddPrimitive(primitive);

    return *this;
}

float SdfCanvas::PrimitiveDistance(const Primitive& primitive, const Vec2f& point) const
{
    switch (primitive.type)
    {
    case PrimitiveType::Circle:
        return SdCircle(point, primitive.a, primitive.radius);
    case PrimitiveType::Ring:
        return MathUtil::Abs(SdCircle(point, primitive.a, primitive.radius)) - primitive.halfWidth;
    case PrimitiveType::EllipseRing:
        return SdEllipseRing(point, primitive.a, primitive.b, primitive.halfWidth);
    case PrimitiveType::RoundedBox:
        return SdRoundedBox(point, primitive.a, primitive.b, primitive.radius);
    case PrimitiveType::Segment:
        return SdSegment(point, primitive.a, primitive.b, primitive.radius);
    case PrimitiveType::ConvexPolygon:
    {
        float distance = -MathUtil::Infinity<float>();

        for (uint32 edgeIndex = primitive.firstEdge; edgeIndex < primitive.firstEdge + primitive.numEdges; edgeIndex++)
        {
            const PolygonEdge& edge = m_polygonEdges[edgeIndex];

            distance = MathUtil::Max(distance, (point - edge.origin).Dot(edge.outwardNormal));
        }

        // exact for convex polygons: every edge moves inward by the same amount
        return distance + primitive.halfWidth;
    }
    default:
        return MathUtil::Infinity<float>();
    }
}

float SdfCanvas::Distance(const Vec2f& point) const
{
    float distance = MathUtil::Infinity<float>();

    for (const Primitive& primitive : m_primitives)
    {
        const float primitiveDistance = PrimitiveDistance(primitive, point);

        if (primitive.blend <= 0.0f)
        {
            distance = MathUtil::Min(distance, primitiveDistance);

            continue;
        }

        // polynomial smooth min
        const float h = MathUtil::Max(primitive.blend - MathUtil::Abs(distance - primitiveDistance), 0.0f) / primitive.blend;

        distance = MathUtil::Min(distance, primitiveDistance) - h * h * primitive.blend * 0.25f;
    }

    return distance;
}

Bitmap_RGBA8 SdfCanvas::Rasterize(uint32 size, const SdfRasterStyle& style) const
{
    Bitmap_RGBA8 bitmap(size, size);

    const float pixelWidth = 2.0f / float(size);
    const bool hasOutline = style.outlineWidth > 0.0f && style.outlineOpacity > 0.0f;

    for (uint32 row = 0; row < size; row++)
    {
        const float y = 1.0f - (float(row) + 0.5f) * pixelWidth;

        for (uint32 column = 0; column < size; column++)
        {
            const float x = (float(column) + 0.5f) * pixelWidth - 1.0f;

            const float distance = Distance(Vec2f(x, y));

            const float fill = MathUtil::Clamp(0.5f - distance / pixelWidth, 0.0f, 1.0f);
            const float outline = hasOutline
                ? MathUtil::Clamp(0.5f - (distance - style.outlineWidth) / pixelWidth, 0.0f, 1.0f) * style.outlineOpacity
                : 0.0f;

            const float outlineContribution = outline * (1.0f - fill);
            const float alpha = fill + outlineContribution;

            // transparent texels take the outline shade so mip filtering doesn't bleed white into the edge
            const float shade = alpha > 0.0f
                ? (fill + outlineContribution * style.outlineShade) / alpha
                : style.outlineShade;

            const ubyte shadeByte = ubyte(shade * 255.0f + 0.5f);

            Bitmap_RGBA8::PixelReferenceType pixel = bitmap.GetPixelReference(column, row);
            pixel.SetComponentRaw(0, shadeByte);
            pixel.SetComponentRaw(1, shadeByte);
            pixel.SetComponentRaw(2, shadeByte);
            pixel.SetComponentRaw(3, ubyte(alpha * 255.0f + 0.5f));
        }
    }

    return bitmap;
}

} // namespace Hyperion
