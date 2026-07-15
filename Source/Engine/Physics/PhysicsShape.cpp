/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Vertex.hpp>

#include <PhysicsShape.generated.inl>

namespace Hyperion {

#pragma region ConvexHullPhysicsShape

ConvexHullPhysicsShape::ConvexHullPhysicsShape(Name name, const VertexArrayView& vertexData)
    : PhysicsShape(name, PhysicsShapeType::ConvexHull)
{
    // Must only have position component
    Assert(vertexData.layoutDesc == StaticVertexInputLayout<VT_Position>);
    
    if (vertexData.vertexCount > 0)
    {
        AllocateBlobData(m_vertexData, vertexData.floatData, vertexData.vertexCount * sizeof(float) * 3, alignof(float));
    }
}

ConvexHullPhysicsShape::~ConvexHullPhysicsShape()
{
    FreeBlobData(m_vertexData);
}

void ConvexHullPhysicsShape::SetVertexData(const struct VertexArrayView& vertexData)
{
    FreeBlobData(m_vertexData);

    if (vertexData.vertexCount > 0)
    {
        AllocateBlobData(m_vertexData, vertexData.floatData, vertexData.vertexCount * sizeof(float) * 3, alignof(float));
    }

    MarkDirty();
}

#pragma endregion ConvexHullPhysicsShape

} // namespace Hyperion
