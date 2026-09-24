/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Scene/LightmapVolume.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <algorithm>
#include <cmath>

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace Hyperion {

namespace Baking {

static constexpr float UnwrapNominalResolution = 128.0f;
static constexpr uint32 UnwrapPadding = 2;

static constexpr uint32 RectGutter = 1;

static constexpr float MinRectScaleTexels = 4.0f;

static constexpr float TargetAtlasOccupancy = 0.8f;

static constexpr uint32 MaxPackAttempts = 24;

static constexpr uint32 InvalidChartId = ~0u;

///Helpers
namespace {

size_t GetPacketOffset(VertexInputLayoutDesc layout, uint8 packetType)
{
    size_t offset = 0;

    FOR_EACH_BIT(layout.mask, bit)
    {
        const uint8 type = uint8(1u << bit);

        if (type == packetType)
        {
            return offset;
        }

        offset += VertexUtils::PacketSize(VertexType(type));
    }

    return ~size_t(0);
}

void ConvertVertex(const ubyte* src, VertexInputLayoutDesc srcLayout, ubyte* dst, VertexInputLayoutDesc dstLayout)
{
    size_t dstOffset = 0;

    FOR_EACH_BIT(dstLayout.mask, bit)
    {
        const uint8 type = uint8(1u << bit);
        const size_t packetSize = VertexUtils::PacketSize(VertexType(type));

        if (srcLayout.mask & type)
        {
            Memory::Copy(dst + dstOffset, src + GetPacketOffset(srcLayout, type), packetSize);
        }
        else
        {
            Memory::Zero(dst + dstOffset, packetSize);
        }

        dstOffset += packetSize;
    }
}

HYP_FORCE_INLINE Vec3f ReadPosition(const ubyte* vertexBytes, size_t vertexSize, size_t positionOffset, uint32 vertexIndex)
{
    return reinterpret_cast<const TVertexPacket<VT_Position>*>(vertexBytes + vertexIndex * vertexSize + positionOffset)->GetPosition();
}

HYP_FORCE_INLINE Vec2f ReadUV1(const ubyte* vertexBytes, size_t vertexSize, size_t uv1Offset, uint32 vertexIndex)
{
    return reinterpret_cast<const TVertexPacket<VT_UV1>*>(vertexBytes + vertexIndex * vertexSize + uv1Offset)->GetUV1();
}

HYP_FORCE_INLINE bool IsTexelLit(const LightmapTexel& texel)
{
    return texel.pRay != nullptr && texel.color0.w > 0.0f;
}

///a wall resting on a chart leaves a band of unlit texels in it
///and light from one side must not be blurred across to the other
bool IsBlurPathClear(const LightmapTexel* atlasTexels, uint32 width, int fromX, int fromY, int toX, int toY)
{
    const int deltaX = toX - fromX;
    const int deltaY = toY - fromY;
    const int numSteps = MathUtil::Max(MathUtil::Abs(deltaX), MathUtil::Abs(deltaY));

    for (int step = 1; step < numSteps; step++)
    {
        const int x = fromX + MathUtil::Round<float, int>(float(deltaX * step) / float(numSteps));
        const int y = fromY + MathUtil::Round<float, int>(float(deltaY * step) / float(numSteps));

        if (!IsTexelLit(atlasTexels[uint32(x) + uint32(y) * width]))
        {
            return false;
        }
    }

    return true;
}

} // namespace anonymous

#pragma region BakeData<LightmapVolume>

BakeData<LightmapVolume>::BakeData(Span<const BakeEntity> bakeEntities, float texelsPerUnit)
    : BakeDataBase(bakeEntities),
      m_texelsPerUnit(texelsPerUnit)
{
    m_entityMeshIndices.Resize(bakeEntities.Size());
    m_entityRects.Resize(bakeEntities.Size());

    Map<ObjId<Mesh>, uint32> meshSnapshotIndices;

    for (size_t entityIndex = 0; entityIndex < bakeEntities.Size(); entityIndex++)
    {
        const Handle<Mesh>& mesh = bakeEntities[entityIndex].mesh;
        Assert(mesh.IsValid());

        auto meshSnapshotIt = meshSnapshotIndices.Find(mesh->Id());

        if (meshSnapshotIt != meshSnapshotIndices.End())
        {
            m_entityMeshIndices[entityIndex] = meshSnapshotIt->second;

            continue;
        }

        const uint32 meshSnapshotIndex = uint32(m_meshes.Size());

        meshSnapshotIndices.Set(mesh->Id(), meshSnapshotIndex);
        m_entityMeshIndices[entityIndex] = meshSnapshotIndex;

        MeshSnapshot& meshSnapshot = m_meshes.EmplaceBack();
        meshSnapshot.mesh = mesh;

        auto readScope = mesh->GetReadScope();

        if (!readScope)
        {
            meshSnapshot.valid = false;

            continue;
        }

        const VertexArrayView vertexData = mesh->GetVertexData(0);
        const Span<const ubyte> indexData = mesh->GetIndexData(0);

        const size_t indexSize = GpuElemTypeSize(mesh->GetMeshAttributes().indexBufferElemType);

        if (!vertexData.floatData || vertexData.vertexCount == 0 || indexData.Size() == 0 || indexSize == 0 || indexSize > sizeof(uint32))
        {
            HYP_LOG(Lightmap, Warning, "Mesh '{}' has no usable LOD 0 data; skipping it for lightmapping", mesh->GetName());

            meshSnapshot.valid = false;

            continue;
        }

        meshSnapshot.layout = vertexData.layoutDesc;

        const size_t vertexDataSize = vertexData.layoutDesc.VertexSize() * vertexData.vertexCount;
        AssertDebug(vertexDataSize % sizeof(float) == 0);

        meshSnapshot.vertices.Resize(vertexDataSize / sizeof(float));
        Memory::Copy(meshSnapshot.vertices.Data(), vertexData.floatData, vertexDataSize);

        const size_t numIndices = indexData.Size() / indexSize;
        meshSnapshot.indices.Resize(numIndices);

        for (size_t index = 0; index < numIndices; index++)
        {
            uint32 value = 0;
            Memory::Copy(&value, indexData.Data() + index * indexSize, indexSize);

            meshSnapshot.indices[index] = value;
        }

        meshSnapshot.needsUnwrap = !mesh->HasValidLightmapUVs();
    }
}

void BakeData<LightmapVolume>::UseExistingPacking(const LightmapVolume& volume, Array<EntityRect, BakerAllocator>&& entityRects)
{
    Assert(entityRects.Size() == m_entityRects.Size());
    Assert(volume.GetAtlases().Size() > 0);

    m_reuseExistingPacking = true;
    m_entityRects = std::move(entityRects);

    m_atlasCount = volume.NumUsedAtlases();
    m_atlasDimensions = volume.GetAtlases()[0].atlasDimensions;
}

bool BakeData<LightmapVolume>::AnyMeshNeedsUnwrap() const
{
    for (const MeshSnapshot& meshSnapshot : m_meshes)
    {
        if (meshSnapshot.valid && meshSnapshot.needsUnwrap)
        {
            return true;
        }
    }

    return false;
}

Result BakeData<LightmapVolume>::Build()
{
    if (m_meshes.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No mesh data to build lightmap UVs from");
    }

    for (MeshSnapshot& meshSnapshot : m_meshes)
    {
        if (!meshSnapshot.valid)
        {
            continue;
        }

        if (meshSnapshot.needsUnwrap)
        {
            // an existing packing is only reused when every mesh's UV1 is still valid
            AssertDebug(!m_reuseExistingPacking);

            if (Result unwrapResult = UnwrapMesh(meshSnapshot); unwrapResult.HasError())
            {
                HYP_LOG(Lightmap, Warning, "Could not generate lightmap UVs for mesh '{}': {}", meshSnapshot.mesh->GetName(), unwrapResult.GetError().GetMessage());

                meshSnapshot.valid = false;

                continue;
            }

            meshSnapshot.unwrapped = true;
        }

        ComputeMeshCharts(meshSnapshot);
    }

    if (!m_reuseExistingPacking)
    {
        if (Result packResult = PackEntities(); packResult.HasError())
        {
            return packResult;
        }
    }

    if (m_atlasCount == 0)
    {
        return HYP_MAKE_ERROR(Error, "No entities could be packed into the lightmap atlas");
    }

    dimensions = Vec3u { m_atlasDimensions.x, m_atlasDimensions.y, 1 };

    const uint32 texelsPerAtlas = m_atlasDimensions.x * m_atlasDimensions.y;

    texels.Resize(m_atlasCount * texelsPerAtlas);
    m_rays.Resize(m_atlasCount * texelsPerAtlas);

    // chart ids stay unique across entities so dilation never mixes two entities' texels
    uint32 chartBase = 0;

    for (uint32 entityIndex = 0; entityIndex < uint32(m_entityRects.Size()); entityIndex++)
    {
        const MeshSnapshot& meshSnapshot = GetMeshSnapshotForEntity(entityIndex);

        if (!m_entityRects[entityIndex].valid || !meshSnapshot.valid)
        {
            continue;
        }

        RasterizeEntity(entityIndex, chartBase);

        chartBase += meshSnapshot.numCharts;
    }

    return {};
}

Result BakeData<LightmapVolume>::UnwrapMesh(MeshSnapshot& meshSnapshot) const
{
#ifdef HYP_XATLAS
    const size_t vertexSize = meshSnapshot.layout.VertexSize();
    const uint32 numVertices = uint32(meshSnapshot.vertices.ByteSize() / vertexSize);

    const size_t positionOffset = GetPacketOffset(meshSnapshot.layout, VT_Position);
    const size_t normalOffset = GetPacketOffset(meshSnapshot.layout, VT_Normal);
    const size_t uv0Offset = GetPacketOffset(meshSnapshot.layout, VT_UV0);

    if (positionOffset == ~size_t(0))
    {
        return HYP_MAKE_ERROR(Error, "Mesh has no positions");
    }

    if (meshSnapshot.indices.Size() % 3 != 0)
    {
        return HYP_MAKE_ERROR(Error, "Mesh is not a triangle list");
    }

    const ubyte* vertexBytes = reinterpret_cast<const ubyte*>(meshSnapshot.vertices.Data());

    float surfaceArea = 0.0f;

    for (size_t index = 0; index + 2 < meshSnapshot.indices.Size(); index += 3)
    {
        if (meshSnapshot.indices[index] >= numVertices || meshSnapshot.indices[index + 1] >= numVertices || meshSnapshot.indices[index + 2] >= numVertices)
        {
            return HYP_MAKE_ERROR(Error, "Mesh has out of range indices");
        }

        const Vec3f p0 = ReadPosition(vertexBytes, vertexSize, positionOffset, meshSnapshot.indices[index]);
        const Vec3f p1 = ReadPosition(vertexBytes, vertexSize, positionOffset, meshSnapshot.indices[index + 1]);
        const Vec3f p2 = ReadPosition(vertexBytes, vertexSize, positionOffset, meshSnapshot.indices[index + 2]);

        surfaceArea += (p1 - p0).Cross(p2 - p0).Length() * 0.5f;
    }

    if (!(surfaceArea > 1e-12f))
    {
        return HYP_MAKE_ERROR(Error, "Mesh has no surface area");
    }

    xatlas::MeshDecl meshDecl;
    meshDecl.vertexCount = numVertices;
    meshDecl.vertexPositionData = vertexBytes + positionOffset;
    meshDecl.vertexPositionStride = uint32(vertexSize);

    if (normalOffset != ~size_t(0))
    {
        meshDecl.vertexNormalData = vertexBytes + normalOffset;
        meshDecl.vertexNormalStride = uint32(vertexSize);
    }

    if (uv0Offset != ~size_t(0))
    {
        meshDecl.vertexUvData = vertexBytes + uv0Offset;
        meshDecl.vertexUvStride = uint32(vertexSize);
    }

    meshDecl.indexData = meshSnapshot.indices.Data();
    meshDecl.indexCount = uint32(meshSnapshot.indices.Size());
    meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

    xatlas::Atlas* atlas = xatlas::Create();

    HYP_DEFER({ xatlas::Destroy(atlas); });

    if (xatlas::AddMesh(atlas, meshDecl) != xatlas::AddMeshError::Success)
    {
        return HYP_MAKE_ERROR(Error, "xatlas rejected the mesh");
    }

    xatlas::ComputeCharts(atlas);

    // unwrapped in the mesh's own space and sized off its own area, so every entity using it shares the same UV1
    xatlas::PackOptions packOptions {};
    packOptions.padding = UnwrapPadding;
    packOptions.bilinear = true;
    packOptions.resolution = 0;
    packOptions.texelsPerUnit = UnwrapNominalResolution / MathUtil::Sqrt(surfaceArea);

    xatlas::PackCharts(atlas, packOptions);

    if (atlas->meshCount != 1 || atlas->atlasCount != 1 || atlas->width == 0 || atlas->height == 0)
    {
        return HYP_MAKE_ERROR(Error, "xatlas did not produce a single atlas");
    }

    const xatlas::Mesh& atlasMesh = atlas->meshes[0];

    // square-normalized so a uniform scale maps UV1 into a rect without stretching texels
    const float uvNormalizeFactor = 1.0f / float(MathUtil::Max(atlas->width, atlas->height));

    const VertexInputLayoutDesc newLayout { uint8(meshSnapshot.layout.mask | VT_UV1) };
    const size_t newVertexSize = newLayout.VertexSize();
    const size_t uv1Offset = GetPacketOffset(newLayout, VT_UV1);

    Array<float, BakerAllocator> newVertices;
    newVertices.Resize(atlasMesh.vertexCount * newVertexSize / sizeof(float));

    ubyte* newVertexBytes = reinterpret_cast<ubyte*>(newVertices.Data());

    for (uint32 vertexIndex = 0; vertexIndex < atlasMesh.vertexCount; vertexIndex++)
    {
        const xatlas::Vertex& atlasVertex = atlasMesh.vertexArray[vertexIndex];
        ubyte* dst = newVertexBytes + vertexIndex * newVertexSize;

        ConvertVertex(vertexBytes + atlasVertex.xref * vertexSize, meshSnapshot.layout, dst, newLayout);

        reinterpret_cast<TVertexPacket<VT_UV1>*>(dst + uv1Offset)->SetUV1(Vec2f(atlasVertex.uv[0], atlasVertex.uv[1]) * uvNormalizeFactor);
    }

    Array<uint32, BakerAllocator> newIndices;
    newIndices.Resize(atlasMesh.indexCount);

    Memory::Copy(newIndices.Data(), atlasMesh.indexArray, atlasMesh.indexCount * sizeof(uint32));

    meshSnapshot.layout = newLayout;
    meshSnapshot.vertices = std::move(newVertices);
    meshSnapshot.indices = std::move(newIndices);

    return {};
#else
    return HYP_MAKE_ERROR(Error, "Built without xatlas");
#endif
}

void BakeData<LightmapVolume>::ComputeMeshCharts(MeshSnapshot& meshSnapshot) const
{
    const size_t vertexSize = meshSnapshot.layout.VertexSize();
    const uint32 numVertices = uint32(meshSnapshot.vertices.ByteSize() / vertexSize);
    const uint32 numTriangles = uint32(meshSnapshot.indices.Size() / 3);

    const size_t uv1Offset = GetPacketOffset(meshSnapshot.layout, VT_UV1);

    if (uv1Offset == ~size_t(0) || numTriangles == 0)
    {
        meshSnapshot.valid = false;

        return;
    }

    const ubyte* vertexBytes = reinterpret_cast<const ubyte*>(meshSnapshot.vertices.Data());

    Array<uint32, BakerAllocator> chartRoots;
    chartRoots.Resize(numVertices);

    for (uint32 vertexIndex = 0; vertexIndex < numVertices; vertexIndex++)
    {
        chartRoots[vertexIndex] = vertexIndex;
    }

    auto findRoot = [&chartRoots](uint32 index) -> uint32
    {
        while (chartRoots[index] != index)
        {
            chartRoots[index] = chartRoots[chartRoots[index]];
            index = chartRoots[index];
        }

        return index;
    };

    // unwrapping split vertices along chart seams, so triangles sharing a vertex share a chart
    for (uint32 triangleIndex = 0; triangleIndex < numTriangles; triangleIndex++)
    {
        const uint32* triangle = &meshSnapshot.indices[triangleIndex * 3];

        if (triangle[0] >= numVertices || triangle[1] >= numVertices || triangle[2] >= numVertices)
        {
            meshSnapshot.valid = false;

            return;
        }

        const uint32 rootA = findRoot(triangle[0]);

        chartRoots[findRoot(triangle[1])] = rootA;
        chartRoots[findRoot(triangle[2])] = rootA;
    }

    Array<uint32, BakerAllocator> rootCharts;
    rootCharts.Resize(numVertices);

    for (uint32 vertexIndex = 0; vertexIndex < numVertices; vertexIndex++)
    {
        rootCharts[vertexIndex] = InvalidChartId;
    }

    meshSnapshot.triangleCharts.Resize(numTriangles);
    meshSnapshot.numCharts = 0;

    Vec2f uvExtent = Vec2f::Zero();
    float uvArea = 0.0f;

    for (uint32 triangleIndex = 0; triangleIndex < numTriangles; triangleIndex++)
    {
        const uint32* triangle = &meshSnapshot.indices[triangleIndex * 3];

        const uint32 root = findRoot(triangle[0]);

        if (rootCharts[root] == InvalidChartId)
        {
            rootCharts[root] = meshSnapshot.numCharts++;
        }

        meshSnapshot.triangleCharts[triangleIndex] = rootCharts[root];

        const Vec2f uv0 = ReadUV1(vertexBytes, vertexSize, uv1Offset, triangle[0]);
        const Vec2f uv1 = ReadUV1(vertexBytes, vertexSize, uv1Offset, triangle[1]);
        const Vec2f uv2 = ReadUV1(vertexBytes, vertexSize, uv1Offset, triangle[2]);

        uvExtent = Vec2f::Max(uvExtent, Vec2f::Max(uv0, Vec2f::Max(uv1, uv2)));

        const Vec2f edgeA = uv1 - uv0;
        const Vec2f edgeB = uv2 - uv0;

        uvArea += MathUtil::Abs(edgeA.x * edgeB.y - edgeA.y * edgeB.x) * 0.5f;
    }

    meshSnapshot.uvExtent = Vec2f(MathUtil::Min(uvExtent.x, 1.0f), MathUtil::Min(uvExtent.y, 1.0f));
    meshSnapshot.uvArea = uvArea;

    if (!(meshSnapshot.uvArea > 0.0f) || meshSnapshot.uvExtent.x <= 0.0f || meshSnapshot.uvExtent.y <= 0.0f)
    {
        HYP_LOG(Lightmap, Warning, "Mesh '{}' has empty lightmap UVs; skipping it", meshSnapshot.mesh->GetName());

        meshSnapshot.valid = false;
    }
}

Result BakeData<LightmapVolume>::PackEntities()
{
    const uint32 numEntities = uint32(bakeEntities.Size());

    // texels per UV1 unit each entity would need to hit the volume's texel density
    Array<float, BakerAllocator> desiredScales;
    desiredScales.Resize(numEntities);

    Array<uint32, BakerAllocator> packOrder;

    for (uint32 entityIndex = 0; entityIndex < numEntities; entityIndex++)
    {
        desiredScales[entityIndex] = 0.0f;

        const MeshSnapshot& meshSnapshot = GetMeshSnapshotForEntity(entityIndex);

        if (!meshSnapshot.valid)
        {
            continue;
        }

        const size_t vertexSize = meshSnapshot.layout.VertexSize();
        const size_t positionOffset = GetPacketOffset(meshSnapshot.layout, VT_Position);
        const ubyte* vertexBytes = reinterpret_cast<const ubyte*>(meshSnapshot.vertices.Data());

        const Mat4f& modelMatrix = bakeEntities[entityIndex].transformMatrix;

        float worldArea = 0.0f;

        for (size_t index = 0; index + 2 < meshSnapshot.indices.Size(); index += 3)
        {
            const Vec3f p0 = modelMatrix.TransformVector(ReadPosition(vertexBytes, vertexSize, positionOffset, meshSnapshot.indices[index]));
            const Vec3f p1 = modelMatrix.TransformVector(ReadPosition(vertexBytes, vertexSize, positionOffset, meshSnapshot.indices[index + 1]));
            const Vec3f p2 = modelMatrix.TransformVector(ReadPosition(vertexBytes, vertexSize, positionOffset, meshSnapshot.indices[index + 2]));

            worldArea += (p1 - p0).Cross(p2 - p0).Length() * 0.5f;
        }

        if (!(worldArea > 0.0f))
        {
            continue;
        }

        desiredScales[entityIndex] = MathUtil::Sqrt(worldArea / meshSnapshot.uvArea) * m_texelsPerUnit;

        packOrder.PushBack(entityIndex);
    }

    if (packOrder.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No entities with usable lightmap UVs");
    }

    const float maxRectSide = float(MathUtil::Min(m_atlasDimensions.x, m_atlasDimensions.y) - 2 * RectGutter);

    Array<float, BakerAllocator> scales;
    scales.Resize(numEntities);

    Array<Vec2u, BakerAllocator> rectDimensions;
    rectDimensions.Resize(numEntities);

    auto computeRects = [&](float densityFactor) -> double
    {
        double totalArea = 0.0;

        for (uint32 entityIndex : packOrder)
        {
            const MeshSnapshot& meshSnapshot = GetMeshSnapshotForEntity(entityIndex);

            const float maxExtent = MathUtil::Max(meshSnapshot.uvExtent.x, meshSnapshot.uvExtent.y);

            // whole texels, since that's what EntityShaderData carries - a fractional scale would drift up to half a texel at the rect's far edge
            const float scale = std::floor(MathUtil::Clamp(desiredScales[entityIndex] * densityFactor, MinRectScaleTexels, maxRectSide / maxExtent));

            scales[entityIndex] = scale;
            rectDimensions[entityIndex] = Vec2u {
                uint32(std::ceil(meshSnapshot.uvExtent.x * scale)) + 2 * RectGutter,
                uint32(std::ceil(meshSnapshot.uvExtent.y * scale)) + 2 * RectGutter
            };

            totalArea += double(rectDimensions[entityIndex].x) * double(rectDimensions[entityIndex].y);
        }

        return totalArea;
    };

    const double capacity = double(m_atlasDimensions.x) * double(m_atlasDimensions.y) * double(MaxAtlasesPerLightmapVolume) * double(TargetAtlasOccupancy);

    // everything is scaled down together when it doesn't fit, so relative texel density holds across the volume
    float densityFactor = 1.0f;

    if (const double totalArea = computeRects(densityFactor); totalArea > capacity)
    {
        densityFactor = float(std::sqrt(capacity / totalArea));

        HYP_LOG(Lightmap, Info, "Lightmap rects need {} texels but the volume holds about {}; scaling texel density by {}",
            totalArea, capacity, densityFactor);
    }

    for (uint32 attempt = 0; attempt < MaxPackAttempts; attempt++, densityFactor *= 0.85f)
    {
        computeRects(densityFactor);

        // tallest first packs a skyline best
        std::sort(packOrder.Begin(), packOrder.End(), [&rectDimensions](uint32 lhs, uint32 rhs)
            {
                if (rectDimensions[lhs].y != rectDimensions[rhs].y)
                {
                    return rectDimensions[lhs].y > rectDimensions[rhs].y;
                }

                if (rectDimensions[lhs].x != rectDimensions[rhs].x)
                {
                    return rectDimensions[lhs].x > rectDimensions[rhs].x;
                }

                return lhs < rhs;
            });

        Array<LightmapVolumeAtlas> atlases;
        Array<EntityRect, BakerAllocator> entityRects;
        entityRects.Resize(numEntities);

        bool allPlaced = true;

        for (uint32 entityIndex : packOrder)
        {
            bool placed = false;

            for (uint32 atlasIndex = 0; atlasIndex < MaxAtlasesPerLightmapVolume && !placed; atlasIndex++)
            {
                if (atlasIndex >= atlases.Size())
                {
                    atlases.EmplaceBack(m_atlasDimensions);
                }

                LightmapElement* element = nullptr;
                uint32 elementIndex = ~0u;

                if (!atlases[atlasIndex].AddElement(rectDimensions[entityIndex], element, elementIndex, /* shrinkToFit */ false))
                {
                    continue;
                }

                AssertDebug(elementIndex < UINT16_MAX);

                element->id = LightmapElement::MakeId(uint16(atlasIndex), uint16(elementIndex));
                element->offsetUV = Vec2f(element->offsetCoords + Vec2u(RectGutter)) / Vec2f(m_atlasDimensions);
                element->scale = Vec2f(scales[entityIndex]) / Vec2f(m_atlasDimensions);

                EntityRect& entityRect = entityRects[entityIndex];
                entityRect.valid = true;
                entityRect.atlasIndex = uint16(atlasIndex);
                entityRect.elementIndex = uint16(elementIndex);
                entityRect.offsetCoords = element->offsetCoords;
                entityRect.dimensions = element->dimensions;
                entityRect.offsetUV = element->offsetUV;
                entityRect.scale = element->scale;

                placed = true;
            }

            if (!placed)
            {
                allPlaced = false;

                break;
            }
        }

        if (!allPlaced)
        {
            continue;
        }

        m_atlasCount = uint32(atlases.Size());
        m_packedAtlases = std::move(atlases);
        m_entityRects = std::move(entityRects);

        return {};
    }

    return HYP_MAKE_ERROR(Error, "Could not fit the volume's entities into its lightmap atlases");
}

void BakeData<LightmapVolume>::RasterizeEntity(uint32 entityIndex, uint32 chartBase)
{
    const EntityRect& entityRect = m_entityRects[entityIndex];
    const MeshSnapshot& meshSnapshot = GetMeshSnapshotForEntity(entityIndex);

    if (entityRect.atlasIndex >= m_atlasCount)
    {
        return;
    }

    const size_t vertexSize = meshSnapshot.layout.VertexSize();
    const uint32 numVertices = uint32(meshSnapshot.vertices.ByteSize() / vertexSize);
    const uint32 numTriangles = uint32(meshSnapshot.indices.Size() / 3);

    const size_t positionOffset = GetPacketOffset(meshSnapshot.layout, VT_Position);
    const size_t normalOffset = GetPacketOffset(meshSnapshot.layout, VT_Normal);
    const size_t uv1Offset = GetPacketOffset(meshSnapshot.layout, VT_UV1);

    const ubyte* vertexBytes = reinterpret_cast<const ubyte*>(meshSnapshot.vertices.Data());

    const Mat4f& modelMatrix = bakeEntities[entityIndex].transformMatrix;
    const Mat4f normalMatrix = modelMatrix.Inverse().Transpose();

    const Vec2f atlasDimensions { float(m_atlasDimensions.x), float(m_atlasDimensions.y) };
    const Vec2f offsetTexels = entityRect.offsetUV * atlasDimensions;
    const Vec2f scaleTexels = entityRect.scale * atlasDimensions;

    Array<Vec3f, BakerAllocator> worldPositions;
    Array<Vec3f, BakerAllocator> worldNormals;
    Array<Vec2f, BakerAllocator> texelCoords;

    worldPositions.Resize(numVertices);
    worldNormals.Resize(numVertices);
    texelCoords.Resize(numVertices);

    for (uint32 vertexIndex = 0; vertexIndex < numVertices; vertexIndex++)
    {
        worldPositions[vertexIndex] = modelMatrix.TransformVector(ReadPosition(vertexBytes, vertexSize, positionOffset, vertexIndex));

        if (normalOffset != ~size_t(0))
        {
            const Vec3f localNormal = reinterpret_cast<const TVertexPacket<VT_Normal>*>(vertexBytes + vertexIndex * vertexSize + normalOffset)->GetNormal();

            worldNormals[vertexIndex] = normalMatrix.TransformVector(Vec4f(localNormal, 0.0f)).GetXYZ();
        }
        else
        {
            worldNormals[vertexIndex] = Vec3f::Zero();
        }

        texelCoords[vertexIndex] = offsetTexels + ReadUV1(vertexBytes, vertexSize, uv1Offset, vertexIndex) * scaleTexels;
    }

    // never write outside the rect, even if UV1 strays past [0, 1]
    const int32 rectMinX = int32(entityRect.offsetCoords.x);
    const int32 rectMinY = int32(entityRect.offsetCoords.y);
    const int32 rectMaxX = int32(entityRect.offsetCoords.x + entityRect.dimensions.x) - 1;
    const int32 rectMaxY = int32(entityRect.offsetCoords.y + entityRect.dimensions.y) - 1;

    const uint32 atlasWidth = m_atlasDimensions.x;
    const uint32 atlasTexelOffset = uint32(entityRect.atlasIndex) * m_atlasDimensions.x * m_atlasDimensions.y;

    const ObjId<Mesh> meshId = meshSnapshot.mesh->Id();

    for (uint32 triangleIndex = 0; triangleIndex < numTriangles; triangleIndex++)
    {
        const uint32 i0 = meshSnapshot.indices[triangleIndex * 3];
        const uint32 i1 = meshSnapshot.indices[triangleIndex * 3 + 1];
        const uint32 i2 = meshSnapshot.indices[triangleIndex * 3 + 2];

        const Vec2f a = texelCoords[i0];
        const Vec2f b = texelCoords[i1];
        const Vec2f c = texelCoords[i2];

        const float denominator = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);

        if (MathUtil::Abs(denominator) < 1e-10f)
        {
            continue;
        }

        const float rcpDenominator = 1.0f / denominator;

        const int32 minX = MathUtil::Max(rectMinX, int32(std::floor(MathUtil::Min(a.x, MathUtil::Min(b.x, c.x)))));
        const int32 minY = MathUtil::Max(rectMinY, int32(std::floor(MathUtil::Min(a.y, MathUtil::Min(b.y, c.y)))));
        const int32 maxX = MathUtil::Min(rectMaxX, int32(std::ceil(MathUtil::Max(a.x, MathUtil::Max(b.x, c.x)))));
        const int32 maxY = MathUtil::Min(rectMaxY, int32(std::ceil(MathUtil::Max(a.y, MathUtil::Max(b.y, c.y)))));

        if (minX > maxX || minY > maxY)
        {
            continue;
        }

        const Vec3f& p0 = worldPositions[i0];
        const Vec3f& p1 = worldPositions[i1];
        const Vec3f& p2 = worldPositions[i2];

        const Vec3f faceNormal = (p1 - p0).Cross(p2 - p0).Normalized();

        const float texelWorldSize = MathUtil::Sqrt((p1 - p0).Cross(p2 - p0).Length() / MathUtil::Abs(denominator));

        const uint32 chartId = chartBase + meshSnapshot.triangleCharts[triangleIndex];

        for (int32 y = minY; y <= maxY; y++)
        {
            for (int32 x = minX; x <= maxX; x++)
            {
                // sampled at the texel center, which is where the shader's bilinear fetch lands
                const Vec2f point { float(x) + 0.5f, float(y) + 0.5f };

                const float w0 = ((b.y - c.y) * (point.x - c.x) + (c.x - b.x) * (point.y - c.y)) * rcpDenominator;
                const float w1 = ((c.y - a.y) * (point.x - c.x) + (a.x - c.x) * (point.y - c.y)) * rcpDenominator;
                const float w2 = 1.0f - w0 - w1;

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                {
                    continue;
                }

                const Vec3f position = p0 * w0 + p1 * w1 + p2 * w2;

                Vec3f normal = worldNormals[i0] * w0 + worldNormals[i1] * w1 + worldNormals[i2] * w2;

                if (normal.LengthSquared() < 1e-12f)
                {
                    normal = faceNormal;
                }
                else
                {
                    normal = normal.Normalized();
                }

                const uint32 texelIndex = atlasTexelOffset + uint32(x) + uint32(y) * atlasWidth;

                LightmapRay& ray = m_rays[texelIndex];
                ray = {
                    .ray = Ray { position, normal },
                    .faceNormal = faceNormal.Dot(normal) < 0.0f ? -faceNormal : faceNormal,
                    .texelWorldSize = texelWorldSize,
                    .meshId = meshId,
                    .triangleIndex = triangleIndex,
                    .texelIndex = texelIndex
                };

                LightmapTexel& texel = texels[texelIndex];
                texel.pRay = &ray;
                texel.chartId = chartId;
            }
        }
    }
}

void BakeData<LightmapVolume>::Blur()
{
    static constexpr int KernelRadius = 3;
    static constexpr float SigmaNormal = 0.25f;
    static constexpr float SigmaLuminance = 0.3f;

    const uint32 width = dimensions.x;
    const uint32 height = dimensions.y;

    if (width == 0 || height == 0 || texels.Empty())
    {
        return;
    }

    const float texelWorldSize = m_texelsPerUnit > 0.0f ? 1.0f / m_texelsPerUnit : 0.5f;
    const float sigmaPosition = texelWorldSize * float(KernelRadius);

    const float sigmaSpRcp = 1.0f / (2.0f * float(KernelRadius) * float(KernelRadius));
    const float sigmaPosRcp = 1.0f / (2.0f * sigmaPosition * sigmaPosition);
    const float sigmaNrmRcp = 1.0f / (2.0f * SigmaNormal * SigmaNormal);
    const float sigmaLumRcp = 1.0f / (2.0f * SigmaLuminance * SigmaLuminance);

    const Vec3f luminanceWeights { 0.2126f, 0.7152f, 0.0722f };

    const uint32 numTexels = width * height;

    for (uint32 atlasIndex = 0; atlasIndex < m_atlasCount; atlasIndex++)
    {
        const uint32 baseOffset = atlasIndex * numTexels;
        const LightmapTexel* atlasTexels = texels.Data() + baseOffset;

        // Note: Don't use temp allocators here, we're allocating a crapload and it will be too much for the arena.
        Array<Vec4f> norm0(numTexels);
        Array<Vec4f> norm1(numTexels);

        for (uint32 i = 0; i < numTexels; i++)
        {
            if (!IsTexelLit(atlasTexels[i]))
            {
                continue;
            }

            norm0[i] = atlasTexels[i].color0 / atlasTexels[i].color0.w;
            norm0[i].w = 1.0f;

            if (atlasTexels[i].color1.w > 0.0f)
            {
                norm1[i] = atlasTexels[i].color1 / atlasTexels[i].color1.w;
                norm1[i].w = 1.0f;
            }
        }

        Array<Vec4f> out0(numTexels);
        Array<Vec4f> out1(numTexels);

        for (uint32 cy = 0; cy < height; cy++)
        {
            for (uint32 cx = 0; cx < width; cx++)
            {
                const uint32 centerIdx = cx + cy * width;

                if (!IsTexelLit(atlasTexels[centerIdx]))
                {
                    continue;
                }

                const Vec3f centerPos = atlasTexels[centerIdx].pRay->ray.position;
                const Vec3f centerNrm = atlasTexels[centerIdx].pRay->ray.direction;
                const uint32 centerChartId = atlasTexels[centerIdx].chartId;
                const float centerLum = norm0[centerIdx].GetXYZ().Dot(luminanceWeights);

                Vec4f accum0 = Vec4f::Zero();
                Vec4f accum1 = Vec4f::Zero();
                float totalW = 0.0f;

                const int nx0 = MathUtil::Max(0, int(cx) - KernelRadius);
                const int nx1 = MathUtil::Min(int(width) - 1, int(cx) + KernelRadius);
                const int ny0 = MathUtil::Max(0, int(cy) - KernelRadius);
                const int ny1 = MathUtil::Min(int(height) - 1, int(cy) + KernelRadius);

                for (int ny = ny0; ny <= ny1; ny++)
                {
                    for (int nx = nx0; nx <= nx1; nx++)
                    {
                        const uint32 nbIdx = uint32(nx) + uint32(ny) * width;

                        // neighbours in the atlas are only neighbours on the surface within the same chart
                        if (!IsTexelLit(atlasTexels[nbIdx]) || atlasTexels[nbIdx].chartId != centerChartId)
                        {
                            continue;
                        }

                        if (!IsBlurPathClear(atlasTexels, width, int(cx), int(cy), nx, ny))
                        {
                            continue;
                        }

                        const float dx = float(nx - int(cx));
                        const float dy = float(ny - int(cy));
                        const float wSpatial = MathUtil::Exp(-(dx * dx + dy * dy) * sigmaSpRcp);

                        const Vec3f posDiff = atlasTexels[nbIdx].pRay->ray.position - centerPos;
                        const float wPos = MathUtil::Exp(-posDiff.Dot(posDiff) * sigmaPosRcp);

                        const float nDot = MathUtil::Clamp(centerNrm.Dot(atlasTexels[nbIdx].pRay->ray.direction), -1.0f, 1.0f);
                        const float wNrm = MathUtil::Exp(-(1.0f - nDot) * sigmaNrmRcp);

                        // texels pushed out to opposite sides of a thin wall sit side by side in the chart
                        const float nbLum = norm0[nbIdx].GetXYZ().Dot(luminanceWeights);
                        const float lumDiff = MathUtil::Abs(nbLum - centerLum) / MathUtil::Max(MathUtil::Max(nbLum, centerLum), 1e-4f);
                        const float wLum = MathUtil::Exp(-lumDiff * lumDiff * sigmaLumRcp);

                        const float w = wSpatial * wPos * wNrm * wLum;

                        accum0 += norm0[nbIdx] * w;
                        accum1 += norm1[nbIdx] * w;
                        totalW += w;
                    }
                }

                if (totalW > 0.0f)
                {
                    out0[centerIdx] = accum0 / totalW;
                    out1[centerIdx] = accum1 / totalW;
                }
            }
        }

        for (uint32 i = 0; i < numTexels; i++)
        {
            if (!IsTexelLit(texels[baseOffset + i]))
            {
                continue;
            }

            texels[baseOffset + i].color0 = out0[i];
            texels[baseOffset + i].color1 = out1[i];
        }
    }
}

void BakeData<LightmapVolume>::Dilate()
{
    static constexpr int NumPasses = 5;

    const uint32 width = dimensions.x;
    const uint32 height = dimensions.y;

    if (width == 0 || height == 0 || texels.Empty())
    {
        return;
    }

    const uint32 numTexels = width * height;

    static constexpr int offsets[8][2] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 }
    };

    // Allocate upfront
    Array<Vec4f> curr0(numTexels);
    Array<Vec4f> curr1(numTexels);
    Array<uint32> currChartId(numTexels);

    Array<Vec4f> next0(numTexels);
    Array<Vec4f> next1(numTexels);
    Array<uint32> nextChartId(numTexels);

    for (uint32 atlasIndex = 0; atlasIndex < m_atlasCount; atlasIndex++)
    {
        const uint32 baseOffset = atlasIndex * numTexels;

        for (uint32 i = 0; i < numTexels; i++)
        {
            curr0[i] = texels[baseOffset + i].color0;
            curr1[i] = texels[baseOffset + i].color1;
            currChartId[i] = texels[baseOffset + i].chartId;
        }

        for (int pass = 0; pass < NumPasses; pass++)
        {
            for (uint32 i = 0; i < numTexels; i++)
            {
                next0[i] = curr0[i];
                next1[i] = curr1[i];
                nextChartId[i] = currChartId[i];
            }

            bool anyChanged = false;

            for (uint32 cy = 0; cy < height; cy++)
            {
                for (uint32 cx = 0; cx < width; cx++)
                {
                    const uint32 idx = cx + cy * width;

                    // gutter texels and texels the integrator rejected as being inside geometry
                    if (curr0[idx].w > 0.0f)
                    {
                        continue;
                    }

                    // a rejected texel belongs to a chart already, and only takes light from that chart
                    uint32 targetChartId = texels[baseOffset + idx].pRay != nullptr ? currChartId[idx] : InvalidChartId;

                    for (int k = 0; k < 8 && targetChartId == InvalidChartId; k++)
                    {
                        const int nx = int(cx) + offsets[k][0];
                        const int ny = int(cy) + offsets[k][1];

                        if (nx < 0 || ny < 0 || nx >= int(width) || ny >= int(height))
                        {
                            continue;
                        }

                        const uint32 nbIdx = uint32(nx) + uint32(ny) * width;

                        if (curr0[nbIdx].w <= 0.0f)
                        {
                            continue;
                        }

                        targetChartId = currChartId[nbIdx];

                        break;
                    }

                    if (targetChartId == InvalidChartId)
                    {
                        continue;
                    }

                    Vec4f accum0 = Vec4f::Zero();
                    Vec4f accum1 = Vec4f::Zero();

                    int count = 0;

                    for (int k = 0; k < 8; k++)
                    {
                        const int nx = int(cx) + offsets[k][0];
                        const int ny = int(cy) + offsets[k][1];

                        if (nx < 0 || ny < 0 || nx >= int(width) || ny >= int(height))
                        {
                            continue;
                        }

                        const uint32 nbIdx = uint32(nx) + uint32(ny) * width;

                        if (curr0[nbIdx].w <= 0.0f || currChartId[nbIdx] != targetChartId)
                        {
                            continue;
                        }

                        accum0 += curr0[nbIdx];
                        accum1 += curr1[nbIdx];
                        ++count;
                    }

                    if (count > 0)
                    {
                        next0[idx] = accum0 / float(count);
                        next1[idx] = accum1 / float(count);
                        nextChartId[idx] = targetChartId;
                        anyChanged = true;
                    }
                }
            }

            Memory::Copy(curr0.Data(), next0.Data(), next0.ByteSize());
            Memory::Copy(curr1.Data(), next1.Data(), next1.ByteSize());
            Memory::Copy(currChartId.Data(), nextChartId.Data(), nextChartId.ByteSize());

            if (!anyChanged)
            {
                break;
            }
        }

        for (uint32 i = 0; i < numTexels; i++)
        {
            if (texels[baseOffset + i].color0.w <= 0.0f)
            {
                texels[baseOffset + i].color0 = curr0[i];
                texels[baseOffset + i].color1 = curr1[i];
                texels[baseOffset + i].chartId = currChartId[i];
            }
        }
    }
}

auto BakeData<LightmapVolume>::ToBitmapIrradiance(uint32 atlasIndex) const -> ColorBitmap
{
    Assert(atlasIndex < m_atlasCount, "Atlas index out of bounds");
    Assert(texels.Size() >= dimensions.x * dimensions.y * m_atlasCount, "Invalid UV map size");

    const uint32 baseOffset = atlasIndex * dimensions.x * dimensions.y;

    ColorBitmap bitmap(dimensions.x, dimensions.y);

    for (uint32 x = 0; x < dimensions.x; x++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            const uint32 index = baseOffset + x + y * dimensions.x;

            Vec4f color = texels[index].color0;

            if (color.w <= 0.0f)
            {
                continue;
            }

            color /= color.w;

            AssertDebug(!MathUtil::IsNaN(color));

            bitmap.GetPixelReference(x, y).SetRGBA(color);
        }
    }

    return bitmap;
}

auto BakeData<LightmapVolume>::ToBitmapBentNormal(uint32 atlasIndex) const -> BentNormalBitmap
{
    Assert(atlasIndex < m_atlasCount, "Atlas index out of bounds");
    Assert(texels.Size() >= dimensions.x * dimensions.y * m_atlasCount, "Invalid UV map size");

    const uint32 baseOffset = atlasIndex * dimensions.x * dimensions.y;

    BentNormalBitmap bitmap(dimensions.x, dimensions.y);

    for (uint32 x = 0; x < dimensions.x; x++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            const uint32 index = baseOffset + x + y * dimensions.x;

            Vec4f accum = texels[index].bentNormal;

            if (accum.w <= 0.0f)
            {
                continue;
            }

            Vec3f bentNormal = (accum.GetXYZ() / accum.w).Normalized();

            // Encode [-1, 1] normal into [0, 1] for RGBA8 storage.
            const Vec4f color = Vec4f(bentNormal * 0.5f + Vec3f(0.5f), 1.0f);

            AssertDebug(!MathUtil::IsNaN(color));

            bitmap.GetPixelReference(x, y).SetRGBA(color);
        }
    }

    return bitmap;
}

#pragma endregion BakeData<LightmapVolume>

} // namespace Baking

} // namespace Hyperion
