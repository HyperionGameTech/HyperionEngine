/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Physics/ConvexDecomposition.hpp>

#include <Rendering/Mesh.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <vhacd/VHACD.h>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Physics);

static const struct
{
    const char* name;
    uint32 maxHulls;
    uint32 resolution;
} s_convexDecompositionPresets[] = {
    { "Single Hull", 1, 1000 },
    { "Low", 4, 4000 },
    { "Medium", 8, 8000 },
    { "High", 16, 16000 },
    { "Very High", 32, 32000 }
};

uint32 GetNumConvexDecompositionPresets()
{
    return uint32(GetArrayCount(s_convexDecompositionPresets));
}

const char* GetConvexDecompositionPresetName(uint32 presetIndex)
{
    if (presetIndex >= GetNumConvexDecompositionPresets())
    {
        return "";
    }

    return s_convexDecompositionPresets[presetIndex].name;
}

ConvexDecompositionSettings GetConvexDecompositionPreset(uint32 presetIndex)
{
    ConvexDecompositionSettings settings;

    if (presetIndex >= GetNumConvexDecompositionPresets())
    {
        return settings;
    }

    settings.maxHulls = s_convexDecompositionPresets[presetIndex].maxHulls;
    settings.resolution = s_convexDecompositionPresets[presetIndex].resolution;

    return settings;
}

bool IsConvexDecompositionSupported()
{
    return true;
}

namespace {

class ConvexDecompositionCallback final : public VHACD::IVHACD::IUserCallback
{
public:
    ConvexDecompositionCallback(VHACD::IVHACD* decomposer, const Proc<bool(float)>& progressCallback)
        : m_decomposer(decomposer),
          m_progressCallback(progressCallback)
    {
    }

    virtual ~ConvexDecompositionCallback() override = default;

    virtual void Update(
        const double overallProgress,
        const double stageProgress,
        const char* const stage,
        const char* operation) override
    {
        if (!m_progressCallback.IsValid())
        {
            return;
        }

        if (!m_progressCallback(float(overallProgress) / 100.0f))
        {
            m_decomposer->Cancel();
        }
    }

private:
    VHACD::IVHACD* m_decomposer;
    const Proc<bool(float)>& m_progressCallback;
};

} // namespace

TResult<ConvexDecompositionResult> DecomposeMesh(
    const Mesh* mesh,
    const ConvexDecompositionSettings& settings,
    const Proc<bool(float)>& progressCallback)
{
    if (!mesh)
    {
        return HYP_MAKE_ERROR(Error, "No mesh to decompose");
    }

    Array<float> sourcePositions;
    Array<uint32> sourceIndices;

    {
        auto readScope = mesh->GetReadScope();

        const VertexArrayView vertices = mesh->GetVertexData(0);
        const Span<const ubyte> indices = mesh->GetIndexData(0);

        if (!vertices.floatData || vertices.vertexCount == 0 || !indices.Data() || indices.Size() < 3 * sizeof(uint32))
        {
            return HYP_MAKE_ERROR(Error, "Mesh has no LOD 0 data to decompose");
        }

        if (GpuElemTypeSize(mesh->GetMeshDesc().meshAttributes.indexBufferElemType) != sizeof(uint32))
        {
            return HYP_MAKE_ERROR(Error, "Convex decomposition needs 32-bit mesh indices");
        }

        const size_t vertexSizeInFloats = vertices.layoutDesc.VertexSize() / sizeof(float);

        sourcePositions.Resize(vertices.vertexCount * 3);

        for (size_t vertexIndex = 0; vertexIndex < vertices.vertexCount; vertexIndex++)
        {
            const float* vertex = vertices.floatData + vertexIndex * vertexSizeInFloats;

            sourcePositions[vertexIndex * 3 + 0] = vertex[0];
            sourcePositions[vertexIndex * 3 + 1] = vertex[1];
            sourcePositions[vertexIndex * 3 + 2] = vertex[2];
        }

        const size_t numIndices = indices.Size() / sizeof(uint32);

        sourceIndices.Resize(numIndices);
        Memory::Copy(sourceIndices.Data(), indices.Data(), numIndices * sizeof(uint32));
    }

    VHACD::IVHACD* decomposer = VHACD::CreateVHACD();

    if (!decomposer)
    {
        return HYP_MAKE_ERROR(Error, "Failed to create V-HACD instance");
    }

    ConvexDecompositionCallback callback { decomposer, progressCallback };

    VHACD::IVHACD::Parameters parameters;
    parameters.m_callback = &callback;
    parameters.m_maxConvexHulls = MathUtil::Max(settings.maxHulls, 1u);
    parameters.m_resolution = MathUtil::Clamp(settings.resolution, 1000u, 64000000u);
    parameters.m_maxNumVerticesPerCH = MathUtil::Clamp(settings.maxVerticesPerHull, 4u, 256u);
    parameters.m_maxRecursionDepth = MathUtil::Max(settings.maxRecursionDepth, 1u);
    parameters.m_shrinkWrap = settings.shrinkWrap;
    parameters.m_fillMode = VHACD::FillMode::FLOOD_FILL;

    // the caller already runs this on a worker thread, and async mode would return before the hulls exist
    parameters.m_asyncACD = false;

    const bool started = decomposer->Compute(
        sourcePositions.Data(),
        uint32(sourcePositions.Size() / 3),
        sourceIndices.Data(),
        uint32(sourceIndices.Size() / 3),
        parameters);

    ConvexDecompositionResult result;

    if (!started)
    {
        decomposer->Clean();
        decomposer->Release();

        return HYP_MAKE_ERROR(Error, "V-HACD could not start decomposing the mesh");
    }

    const uint32 numHulls = decomposer->GetNConvexHulls();

    for (uint32 hullIndex = 0; hullIndex < numHulls; hullIndex++)
    {
        VHACD::IVHACD::ConvexHull hull;

        if (!decomposer->GetConvexHull(hullIndex, hull) || hull.m_points.size() < 4)
        {
            continue;
        }

        ConvexHullRange range;
        range.firstVertex = uint32(result.positions.Size() / 3);
        range.numVertices = uint32(hull.m_points.size());
        range.firstIndex = uint32(result.indices.Size());
        range.numIndices = uint32(hull.m_triangles.size() * 3);

        for (const VHACD::Vertex& point : hull.m_points)
        {
            result.positions.PushBack(float(point.mX));
            result.positions.PushBack(float(point.mY));
            result.positions.PushBack(float(point.mZ));
        }

        for (const VHACD::Triangle& triangle : hull.m_triangles)
        {
            result.indices.PushBack(triangle.mI0);
            result.indices.PushBack(triangle.mI1);
            result.indices.PushBack(triangle.mI2);
        }

        result.hulls.PushBack(range);
    }

    decomposer->Clean();
    decomposer->Release();

    if (result.hulls.Empty())
    {
        return HYP_MAKE_ERROR(Error, "V-HACD produced no convex hulls");
    }

    HYP_LOG(Physics, Info, "Decomposed mesh {} into {} convex hull(s) ({} vertices)",
        mesh->GetName(), result.hulls.Size(), result.positions.Size() / 3);

    return result;
}

} // namespace Hyperion
