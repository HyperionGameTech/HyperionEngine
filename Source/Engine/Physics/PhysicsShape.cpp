/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Vertex.hpp>

#include <Core/Logging/Logger.hpp>

#include <PhysicsShape.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Physics);

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

void ConvexHullPhysicsShape::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    if (m_vertexData.raw != nullptr || !m_vertexData.key || m_vertexData.size == 0)
    {
        return;
    }

    if (!PageBlobDataFromStorage(m_vertexData))
    {
        (void)PageBlobDataFromLocalFile(m_vertexData, "HULL", alignof(float));
    }
}

void ConvexHullPhysicsShape::UnpageBlobData()
{
    AssetObject::UnpageBlobData();

    AssertBlobDataPersisted(m_vertexData);

    if (!m_vertexData.readOnly)
    {
        FreeBlobData(m_vertexData);
    }

    m_vertexData.raw = nullptr;
}

#pragma endregion ConvexHullPhysicsShape

#pragma region CompoundPhysicsShape

CompoundPhysicsShape::~CompoundPhysicsShape()
{
    FreeBlobData(m_vertexData);
    FreeBlobData(m_indexData);
}

Span<const float> CompoundPhysicsShape::GetHullVertices(uint32 hullIndex) const
{
    if (hullIndex >= m_hulls.Size() || !m_vertexData.raw)
    {
        return {};
    }

    const ConvexHullRange& hull = m_hulls[hullIndex];
    const float* positions = reinterpret_cast<const float*>(m_vertexData.raw);

    return Span<const float>(positions + hull.firstVertex * 3, hull.numVertices * 3);
}

Span<const uint32> CompoundPhysicsShape::GetHullIndices(uint32 hullIndex) const
{
    if (hullIndex >= m_hulls.Size() || !m_indexData.raw)
    {
        return {};
    }

    const ConvexHullRange& hull = m_hulls[hullIndex];
    const uint32* indices = reinterpret_cast<const uint32*>(m_indexData.raw);

    return Span<const uint32>(indices + hull.firstIndex, hull.numIndices);
}

void CompoundPhysicsShape::SetHulls(Span<const float> positions, Span<const uint32> indices, Span<const ConvexHullRange> hulls)
{
    FreeBlobData(m_vertexData);
    FreeBlobData(m_indexData);

    m_vertexData = BlobDataReference {};
    m_indexData = BlobDataReference {};

    m_hulls.Clear();

    if (positions.Size() != 0)
    {
        AllocateBlobData(m_vertexData, positions.Data(), positions.Size() * sizeof(float), alignof(float));
    }

    if (indices.Size() != 0)
    {
        AllocateBlobData(m_indexData, indices.Data(), indices.Size() * sizeof(uint32), alignof(uint32));
    }

    m_hulls.Reserve(hulls.Size());

    for (const ConvexHullRange& hull : hulls)
    {
        m_hulls.PushBack(hull);
    }

    Invalidate();

    MarkDirty();
}

void CompoundPhysicsShape::SetDecompositionSettings(const ConvexDecompositionSettings& settings)
{
    m_decompositionSettings = settings;

    MarkDirty();
}

void CompoundPhysicsShape::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    if (m_vertexData.raw == nullptr && m_vertexData.key && m_vertexData.size != 0)
    {
        if (!PageBlobDataFromStorage(m_vertexData))
        {
            (void)PageBlobDataFromLocalFile(m_vertexData, "CHV", alignof(float));
        }
    }

    if (m_indexData.raw == nullptr && m_indexData.key && m_indexData.size != 0)
    {
        if (!PageBlobDataFromStorage(m_indexData))
        {
            (void)PageBlobDataFromLocalFile(m_indexData, "CHI", alignof(uint32));
        }
    }
}

void CompoundPhysicsShape::UnpageBlobData()
{
    AssetObject::UnpageBlobData();

    for (BlobDataReference* reference : { &m_vertexData, &m_indexData })
    {
        AssertBlobDataPersisted(*reference);

        if (!reference->readOnly)
        {
            FreeBlobData(*reference);
        }

        reference->raw = nullptr;
    }
}

#pragma endregion CompoundPhysicsShape

#pragma region HeightFieldPhysicsShape

HeightFieldPhysicsShape::HeightFieldPhysicsShape(Name name, Span<const float> heights, uint32 numSamplesXZ)
    : PhysicsShape(name, PhysicsShapeType::HeightField)
{
    SetHeights(heights, numSamplesXZ);
}

void HeightFieldPhysicsShape::SetHeights(Span<const float> heights, uint32 numSamplesXZ)
{
    if (heights.Size() != size_t(numSamplesXZ) * size_t(numSamplesXZ))
    {
        HYP_LOG(Physics, Warning, "HeightFieldPhysicsShape '{}' expected {}x{} height samples but got {}; update ignored",
            GetName(), numSamplesXZ, numSamplesXZ, heights.Size());

        return;
    }

    m_heights.Resize(heights.Size());

    for (uint32 i = 0; i < heights.Size(); i++)
    {
        m_heights[i] = heights[i];
    }

    m_numSamples = numSamplesXZ;

    MarkDirty();
}

#pragma endregion HeightFieldPhysicsShape

} // namespace Hyperion
