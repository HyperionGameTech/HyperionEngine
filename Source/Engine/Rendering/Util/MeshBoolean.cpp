/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/MeshBoolean.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#if defined(HYP_MANIFOLD) && HYP_MANIFOLD
#include <manifold/manifold.h>
#endif

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

static constexpr uint8 BooleanLayoutMask = VT_Position | VT_Normal | VT_UV0 | VT_UV1;

bool MeshBoolean::IsSupported()
{
#if defined(HYP_MANIFOLD) && HYP_MANIFOLD
    return true;
#else
    return false;
#endif
}

bool MeshBoolean::IsLayoutSupported(const VertexInputLayoutDesc& inputLayout)
{
    return (inputLayout.mask & VT_Position) != 0
        && (inputLayout.mask & ~BooleanLayoutMask) == 0;
}

const char* MeshBoolean::GetErrorMessage(MeshBooleanError error)
{
    switch (error)
    {
    case MeshBooleanError::None:
        return "No error";
    case MeshBooleanError::NotSupported:
        return "Built without manifold - mesh booleans are unavailable";
    case MeshBooleanError::UnsupportedLayout:
        return "Mesh needs triangles with 32-bit indices, and only position, normal and UV channels";
    case MeshBooleanError::MissingData:
        return "Mesh has no LOD 0 data";
    case MeshBooleanError::TargetNotClosed:
        return "Target mesh is not a closed solid";
    case MeshBooleanError::BrushNotClosed:
        return "Brush mesh is not a closed solid";
    case MeshBooleanError::EmptyResult:
        return "The operation would leave nothing behind";
    case MeshBooleanError::Failed:
        return "The boolean operation failed";
    }

    return "";
}

#if defined(HYP_MANIFOLD) && HYP_MANIFOLD

namespace {

static constexpr float BrushProjectedUVScale = 0.5f;
static constexpr float WeldToleranceScale = 1e-5f;

struct SolidSource
{
    manifold::MeshGL meshGL;

    bool isInsideOut = false;
};

static size_t GetAttributeOffsetInFloats(const VertexInputLayoutDesc& inputLayout, VertexType vertexType)
{
    size_t offset = 0;

    for (uint8 bit = VT_Position; bit != 0 && bit < vertexType; bit <<= 1)
    {
        if (inputLayout.mask & bit)
        {
            offset += VertexUtils::PacketSize(VertexType(bit));
        }
    }

    return offset / sizeof(float);
}

static bool CanReadMesh(const Mesh* mesh)
{
    const MeshAttributes& meshAttributes = mesh->GetMeshDesc().meshAttributes;

    return meshAttributes.topology == Topology::Triangles
        && GpuElemTypeSize(meshAttributes.indexBufferElemType) == sizeof(uint32)
        && MeshBoolean::IsLayoutSupported(meshAttributes.inputLayout);
}

static Vec2f ProjectBoxUV(const Vec3f& position, const Vec3f& normal)
{
    const Vec3f absoluteNormal = Vec3f::Abs(normal);

    if (absoluteNormal.x >= absoluteNormal.y && absoluteNormal.x >= absoluteNormal.z)
    {
        return Vec2f(position.z, position.y) * BrushProjectedUVScale;
    }

    if (absoluteNormal.y >= absoluteNormal.z)
    {
        return Vec2f(position.x, position.z) * BrushProjectedUVScale;
    }

    return Vec2f(position.x, position.y) * BrushProjectedUVScale;
}

static MeshBooleanError BuildSolidSource(
    const MeshBooleanOperand& operand,
    const VertexInputLayoutDesc& outputLayout,
    bool projectUV0,
    SolidSource& outSource)
{
    if (!operand.mesh || !CanReadMesh(operand.mesh))
    {
        return operand.mesh ? MeshBooleanError::UnsupportedLayout : MeshBooleanError::MissingData;
    }

    auto readScope = operand.mesh->GetReadScope();

    const VertexArrayView vertices = operand.mesh->GetVertexData(0);
    const Span<const ubyte> indexBytes = operand.mesh->GetIndexData(0);

    if (!vertices.floatData || vertices.vertexCount == 0 || !indexBytes.Data() || indexBytes.Size() < 3 * sizeof(uint32))
    {
        return MeshBooleanError::MissingData;
    }

    const VertexInputLayoutDesc sourceLayout = vertices.layoutDesc;

    const size_t sourceStride = sourceLayout.VertexSize() / sizeof(float);
    const size_t outputStride = outputLayout.VertexSize() / sizeof(float);

    const bool sourceHasNormal = (sourceLayout.mask & VT_Normal) != 0;
    const bool sourceHasUV0 = (sourceLayout.mask & VT_UV0) != 0;
    const bool sourceHasUV1 = (sourceLayout.mask & VT_UV1) != 0;

    const size_t sourceNormalOffset = GetAttributeOffsetInFloats(sourceLayout, VT_Normal);
    const size_t sourceUV0Offset = GetAttributeOffsetInFloats(sourceLayout, VT_UV0);
    const size_t sourceUV1Offset = GetAttributeOffsetInFloats(sourceLayout, VT_UV1);

    const size_t outputNormalOffset = GetAttributeOffsetInFloats(outputLayout, VT_Normal);
    const size_t outputUV0Offset = GetAttributeOffsetInFloats(outputLayout, VT_UV0);
    const size_t outputUV1Offset = GetAttributeOffsetInFloats(outputLayout, VT_UV1);

    const Mat4f normalMatrix = operand.transform.Transpose().Inverse();

    manifold::MeshGL& meshGL = outSource.meshGL;
    meshGL.numProp = uint32(outputStride);
    meshGL.vertProperties.assign(vertices.vertexCount * outputStride, 0.0f);

    for (size_t vertexIndex = 0; vertexIndex < vertices.vertexCount; vertexIndex++)
    {
        const float* sourceVertex = vertices.floatData + vertexIndex * sourceStride;
        float* outputVertex = meshGL.vertProperties.data() + vertexIndex * outputStride;

        const Vec3f position = operand.transform.TransformVector(Vec4f(sourceVertex[0], sourceVertex[1], sourceVertex[2], 1.0f)).GetXYZ();

        outputVertex[0] = position.x;
        outputVertex[1] = position.y;
        outputVertex[2] = position.z;

        Vec3f normal = Vec3f::UnitY();

        if (sourceHasNormal)
        {
            const float* sourceNormal = sourceVertex + sourceNormalOffset;

            normal = normalMatrix.TransformVector(Vec4f(sourceNormal[0], sourceNormal[1], sourceNormal[2], 0.0f)).GetXYZ().Normalized();
        }

        if (outputLayout.mask & VT_Normal)
        {
            outputVertex[outputNormalOffset + 0] = normal.x;
            outputVertex[outputNormalOffset + 1] = normal.y;
            outputVertex[outputNormalOffset + 2] = normal.z;
        }

        if (outputLayout.mask & VT_UV0)
        {
            Vec2f uv0;

            if (projectUV0 || !sourceHasUV0)
            {
                uv0 = ProjectBoxUV(position, normal);
            }
            else
            {
                uv0 = Vec2f(sourceVertex[sourceUV0Offset], sourceVertex[sourceUV0Offset + 1]);
            }

            outputVertex[outputUV0Offset + 0] = uv0.x;
            outputVertex[outputUV0Offset + 1] = uv0.y;
        }

        if ((outputLayout.mask & VT_UV1) && sourceHasUV1)
        {
            outputVertex[outputUV1Offset + 0] = sourceVertex[sourceUV1Offset];
            outputVertex[outputUV1Offset + 1] = sourceVertex[sourceUV1Offset + 1];
        }
    }

    const uint32* indices = reinterpret_cast<const uint32*>(indexBytes.Data());
    const size_t numIndices = (indexBytes.Size() / sizeof(uint32)) / 3 * 3;

    meshGL.triVerts.assign(indices, indices + numIndices);

    readScope.Reset();

    for (uint32 index : meshGL.triVerts)
    {
        if (index >= vertices.vertexCount)
        {
            return MeshBooleanError::MissingData;
        }
    }

    double signedVolume = 0.0;

    for (size_t triangleStart = 0; triangleStart < meshGL.triVerts.size(); triangleStart += 3)
    {
        const float* p0 = meshGL.vertProperties.data() + meshGL.triVerts[triangleStart + 0] * outputStride;
        const float* p1 = meshGL.vertProperties.data() + meshGL.triVerts[triangleStart + 1] * outputStride;
        const float* p2 = meshGL.vertProperties.data() + meshGL.triVerts[triangleStart + 2] * outputStride;

        signedVolume += double(p0[0]) * (double(p1[1]) * double(p2[2]) - double(p1[2]) * double(p2[1]))
            - double(p0[1]) * (double(p1[0]) * double(p2[2]) - double(p1[2]) * double(p2[0]))
            + double(p0[2]) * (double(p1[0]) * double(p2[1]) - double(p1[1]) * double(p2[0]));
    }

    outSource.isInsideOut = signedVolume < 0.0;

    if (outSource.isInsideOut)
    {
        for (size_t triangleStart = 0; triangleStart < meshGL.triVerts.size(); triangleStart += 3)
        {
            std::swap(meshGL.triVerts[triangleStart + 1], meshGL.triVerts[triangleStart + 2]);
        }
    }

    BoundingBox bounds = BoundingBox::Empty();

    for (size_t vertexIndex = 0; vertexIndex < vertices.vertexCount; vertexIndex++)
    {
        const float* position = meshGL.vertProperties.data() + vertexIndex * outputStride;

        bounds = bounds.Union(Vec3f(position[0], position[1], position[2]));
    }

    const Vec3f extent = bounds.GetExtent();

    meshGL.tolerance = MathUtil::Max(extent.x, MathUtil::Max(extent.y, extent.z)) * WeldToleranceScale;
    meshGL.Merge();
    meshGL.tolerance = 0.0f;

    return MeshBooleanError::None;
}

static manifold::OpType ToManifoldOpType(MeshBooleanOperation operation)
{
    switch (operation)
    {
    case MeshBooleanOperation::Union:
        return manifold::OpType::Add;
    case MeshBooleanOperation::Subtract:
        return manifold::OpType::Subtract;
    case MeshBooleanOperation::Intersect:
        return manifold::OpType::Intersect;
    }

    return manifold::OpType::Add;
}

} // namespace

MeshBooleanError MeshBoolean::ValidateSolid(const Mesh* mesh)
{
    if (!mesh)
    {
        return MeshBooleanError::MissingData;
    }

    SolidSource source;

    if (MeshBooleanError error = BuildSolidSource(MeshBooleanOperand { mesh }, mesh->GetMeshDesc().meshAttributes.inputLayout, false, source); error != MeshBooleanError::None)
    {
        return error;
    }

    const manifold::Manifold solid(source.meshGL);

    return solid.Status() == manifold::Manifold::Error::NoError
        ? MeshBooleanError::None
        : MeshBooleanError::TargetNotClosed;
}

MeshBooleanResult MeshBoolean::Apply(const MeshBooleanOperand& target, const MeshBooleanOperand& brush, MeshBooleanOperation operation)
{
    MeshBooleanResult result;

    if (!target.mesh || !brush.mesh)
    {
        result.error = MeshBooleanError::MissingData;

        return result;
    }

    const MeshAttributes targetAttributes = target.mesh->GetMeshDesc().meshAttributes;
    const VertexInputLayoutDesc outputLayout = targetAttributes.inputLayout;

    SolidSource targetSource;

    if (MeshBooleanError error = BuildSolidSource(target, outputLayout, false, targetSource); error != MeshBooleanError::None)
    {
        result.error = error;

        return result;
    }

    SolidSource brushSource;

    if (MeshBooleanError error = BuildSolidSource(brush, outputLayout, true, brushSource); error != MeshBooleanError::None)
    {
        result.error = error;

        return result;
    }

    const manifold::Manifold targetSolid(targetSource.meshGL);

    if (targetSolid.Status() != manifold::Manifold::Error::NoError)
    {
        HYP_LOG(Rendering, Warning, "Mesh boolean: target {} is not a closed solid (manifold error {})",
            target.mesh->GetName(), int(targetSolid.Status()));

        result.error = MeshBooleanError::TargetNotClosed;

        return result;
    }

    const manifold::Manifold brushSolid(brushSource.meshGL);

    if (brushSolid.Status() != manifold::Manifold::Error::NoError)
    {
        result.error = MeshBooleanError::BrushNotClosed;

        return result;
    }

    const manifold::Manifold combined = targetSolid.Boolean(brushSolid, ToManifoldOpType(operation));

    if (combined.Status() != manifold::Manifold::Error::NoError)
    {
        HYP_LOG(Rendering, Warning, "Mesh boolean on {} failed (manifold error {})", target.mesh->GetName(), int(combined.Status()));

        result.error = MeshBooleanError::Failed;

        return result;
    }

    if (combined.IsEmpty())
    {
        result.error = MeshBooleanError::EmptyResult;

        return result;
    }

    const int normalChannel = (outputLayout.mask & VT_Normal)
        ? int(GetAttributeOffsetInFloats(outputLayout, VT_Normal)) - 3
        : -1;

    const manifold::MeshGL output = combined.GetMeshGL(normalChannel);

    const size_t outputStride = outputLayout.VertexSize() / sizeof(float);

    if (size_t(output.numProp) != outputStride || output.NumTri() == 0)
    {
        result.error = output.NumTri() == 0 ? MeshBooleanError::EmptyResult : MeshBooleanError::Failed;

        return result;
    }

    result.vertexData.Resize(output.vertProperties.size());
    Memory::Copy(result.vertexData.Data(), output.vertProperties.data(), output.vertProperties.size() * sizeof(float));

    result.indices.Resize(output.triVerts.size());

    for (size_t triangleStart = 0; triangleStart < output.triVerts.size(); triangleStart += 3)
    {
        result.indices[triangleStart + 0] = output.triVerts[triangleStart + 0];
        result.indices[triangleStart + 1] = targetSource.isInsideOut ? output.triVerts[triangleStart + 2] : output.triVerts[triangleStart + 1];
        result.indices[triangleStart + 2] = targetSource.isInsideOut ? output.triVerts[triangleStart + 1] : output.triVerts[triangleStart + 2];
    }

    result.meshDesc.meshAttributes = targetAttributes;
    result.meshDesc.lods[0].numVertices = uint32(output.NumVert());
    result.meshDesc.lods[0].numIndices = uint32(result.indices.Size());

    return result;
}

#else

MeshBooleanError MeshBoolean::ValidateSolid(const Mesh* mesh)
{
    return MeshBooleanError::NotSupported;
}

MeshBooleanResult MeshBoolean::Apply(const MeshBooleanOperand& target, const MeshBooleanOperand& brush, MeshBooleanOperation operation)
{
    MeshBooleanResult result;
    result.error = MeshBooleanError::NotSupported;

    return result;
}

#endif // HYP_MANIFOLD

} // namespace Hyperion
