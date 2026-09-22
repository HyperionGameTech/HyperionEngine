/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Span.hpp>

#include <Util/Img/Bitmap.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

struct SdfRasterStyle
{
    float outlineWidth = 0.0f;
    float outlineOpacity = 0.0f;
    float outlineShade = 0.0f;
};

class ENGINE_API SdfCanvas final
{
public:
    SdfCanvas& Circle(const Vec2f& center, float radius);
    SdfCanvas& Ring(const Vec2f& center, float radius, float halfWidth);
    SdfCanvas& EllipseRing(const Vec2f& center, const Vec2f& radii, float halfWidth);
    SdfCanvas& RoundedBox(const Vec2f& center, const Vec2f& halfExtents, float cornerRadius);
    SdfCanvas& Segment(const Vec2f& start, const Vec2f& end, float radius);
    SdfCanvas& ConvexPolygon(Span<const Vec2f> points, float inset = 0.0f);

    /// Shapes added after this are smooth-unioned with everything before them, filleting the joins over roughly \p radius. 0 is a hard union.
    SdfCanvas& Blend(float radius);

    float Distance(const Vec2f& point) const;

    Bitmap_RGBA8 Rasterize(uint32 size, const SdfRasterStyle& style = {}) const;

private:
    enum class PrimitiveType : uint8
    {
        Circle,
        Ring,
        EllipseRing,
        RoundedBox,
        Segment,
        ConvexPolygon
    };

    struct Primitive
    {
        PrimitiveType type;
        Vec2f a;
        Vec2f b;
        float radius = 0.0f;
        float halfWidth = 0.0f;
        uint32 firstEdge = 0;
        uint32 numEdges = 0;
        float blend = 0.0f;
    };

    struct PolygonEdge
    {
        Vec2f origin;
        Vec2f outwardNormal;
    };

    void AddPrimitive(Primitive primitive);

    float PrimitiveDistance(const Primitive& primitive, const Vec2f& point) const;

    Array<Primitive> m_primitives;
    Array<PolygonEdge> m_polygonEdges;

    float m_blend = 0.0f;
};

} // namespace Hyperion
