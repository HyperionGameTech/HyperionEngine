/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/MeshLodGenerator.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#if defined(HYP_MESHOPTIMIZER) && HYP_MESHOPTIMIZER
#include <meshoptimizer.h>
#endif

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

bool MeshLodGenerator::IsSupported()
{
#if defined(HYP_MESHOPTIMIZER) && HYP_MESHOPTIMIZER
    return true;
#else
    return false;
#endif
}

bool MeshLodGenerator::CanGenerate(const Mesh* mesh)
{
    if (!IsSupported() || !mesh)
    {
        return false;
    }

    const MeshDesc& meshDesc = mesh->GetMeshDesc();

    return meshDesc.meshAttributes.topology == Topology::Triangles
        && GpuElemTypeSize(meshDesc.meshAttributes.indexBufferElemType) == sizeof(uint32)
        && meshDesc.lods[0].numIndices >= 3
        && meshDesc.lods[0].numVertices > 0
        && !mesh->IsDynamicMesh();
}

#if defined(HYP_MESHOPTIMIZER) && HYP_MESHOPTIMIZER

namespace {

struct MeshLodSource
{
    VertexInputLayoutDesc inputLayout;
    size_t vertexCount = 0;

    Array<float> vertexData;
    Array<uint32> indices;

    Array<float> positions;
    Array<float> attributes;
    Array<float> attributeWeights;

    uint64 dataHash = 0;
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

static void BuildSimplifierStreams(MeshLodSource& source, const MeshLodGenerationSettings& settings)
{
    const size_t vertexSizeInFloats = source.inputLayout.VertexSize() / sizeof(float);

    source.positions.Resize(source.vertexCount * 3);

    const size_t positionOffset = GetAttributeOffsetInFloats(source.inputLayout, VT_Position);

    for (size_t vertexIndex = 0; vertexIndex < source.vertexCount; vertexIndex++)
    {
        const float* vertex = source.vertexData.Data() + vertexIndex * vertexSizeInFloats + positionOffset;

        source.positions[vertexIndex * 3 + 0] = vertex[0];
        source.positions[vertexIndex * 3 + 1] = vertex[1];
        source.positions[vertexIndex * 3 + 2] = vertex[2];
    }

    struct AttributeStream
    {
        VertexType vertexType;
        uint32 numComponents;
        float weight;
    };

    const AttributeStream candidates[] = {
        { VT_Normal, 3, settings.normalWeight },
        { VT_UV0, 2, settings.uv0Weight },
        { VT_UV1, 2, settings.uv1Weight }
    };

    Array<AttributeStream> streams;

    for (const AttributeStream& candidate : candidates)
    {
        if ((source.inputLayout.mask & candidate.vertexType) && candidate.weight > 0.0f)
        {
            streams.PushBack(candidate);
        }
    }

    if (streams.Empty())
    {
        return;
    }

    size_t numAttributeComponents = 0;

    for (const AttributeStream& stream : streams)
    {
        numAttributeComponents += stream.numComponents;
    }

    source.attributes.Resize(source.vertexCount * numAttributeComponents);
    source.attributeWeights.Reserve(numAttributeComponents);

    for (const AttributeStream& stream : streams)
    {
        for (uint32 component = 0; component < stream.numComponents; component++)
        {
            source.attributeWeights.PushBack(stream.weight);
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < source.vertexCount; vertexIndex++)
    {
        const float* vertex = source.vertexData.Data() + vertexIndex * vertexSizeInFloats;

        size_t destinationOffset = vertexIndex * numAttributeComponents;

        for (const AttributeStream& stream : streams)
        {
            const float* attribute = vertex + GetAttributeOffsetInFloats(source.inputLayout, stream.vertexType);

            for (uint32 component = 0; component < stream.numComponents; component++)
            {
                source.attributes[destinationOffset++] = attribute[component];
            }
        }
    }
}

static Result ReadMeshLodSource(const Mesh* mesh, const MeshLodGenerationSettings& settings, MeshLodSource& outSource)
{
    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertices = mesh->GetVertexData(0);
    const Span<const ubyte> indices = mesh->GetIndexData(0);

    if (!vertices.floatData || vertices.vertexCount == 0 || !indices.Data() || indices.Size() < 3 * sizeof(uint32))
    {
        return HYP_MAKE_ERROR(Error, "Mesh has no LOD 0 data to simplify");
    }

    outSource.inputLayout = vertices.layoutDesc;
    outSource.vertexCount = vertices.vertexCount;

    const size_t vertexDataSizeInFloats = (outSource.inputLayout.VertexSize() / sizeof(float)) * vertices.vertexCount;

    outSource.vertexData.Resize(vertexDataSizeInFloats);
    Memory::Copy(outSource.vertexData.Data(), vertices.floatData, vertexDataSizeInFloats * sizeof(float));

    const size_t numIndices = indices.Size() / sizeof(uint32);

    outSource.indices.Resize(numIndices);
    Memory::Copy(outSource.indices.Data(), indices.Data(), numIndices * sizeof(uint32));

    outSource.dataHash = mesh->ComputeLod0DataHash();

    // the simplifier runs off these copies, so LOD 0's blob data can be released again
    readScope.Reset();

    BuildSimplifierStreams(outSource, settings);

    return {};
}

static void BuildCompactLod(const MeshLodSource& source, Span<const uint32> simplifiedIndices, GeneratedMeshLod& outLod)
{
    const size_t vertexSizeInFloats = source.inputLayout.VertexSize() / sizeof(float);

    Array<uint32> remap;
    remap.Resize(source.vertexCount);

    Array<uint32> indices;
    indices.Resize(simplifiedIndices.Size());
    Memory::Copy(indices.Data(), simplifiedIndices.Data(), simplifiedIndices.Size() * sizeof(uint32));

    meshopt_optimizeVertexCache(indices.Data(), indices.Data(), indices.Size(), source.vertexCount);

    const size_t numUniqueVertices = meshopt_optimizeVertexFetchRemap(
        remap.Data(), indices.Data(), indices.Size(), source.vertexCount);

    outLod.indices.Resize(indices.Size());
    meshopt_remapIndexBuffer(outLod.indices.Data(), indices.Data(), indices.Size(), remap.Data());

    outLod.vertexData.Resize(numUniqueVertices * vertexSizeInFloats);
    meshopt_remapVertexBuffer(
        outLod.vertexData.Data(),
        source.vertexData.Data(),
        source.vertexCount,
        source.inputLayout.VertexSize(),
        remap.Data());

    outLod.desc.numVertices = uint32(numUniqueVertices);
    outLod.desc.numIndices = uint32(outLod.indices.Size());
}

} // namespace

TResult<MeshLodGenerationResult> MeshLodGenerator::Generate(const Mesh* mesh, const MeshLodGenerationSettings& settings)
{
    if (!CanGenerate(mesh))
    {
        return HYP_MAKE_ERROR(Error, "Mesh cannot be simplified - needs triangle topology with 32-bit indices");
    }

    MeshLodSource source;

    if (Result readResult = ReadMeshLodSource(mesh, settings, source); readResult.HasError())
    {
        return readResult.GetError();
    }

    Array<uint32> weldedIndices;
    weldedIndices.Resize(source.indices.Size());

    const meshopt_Stream vertexStream {
        source.vertexData.Data(),
        source.inputLayout.VertexSize(),
        source.inputLayout.VertexSize()
    };

    meshopt_generateShadowIndexBufferMulti(
        weldedIndices.Data(),
        source.indices.Data(),
        source.indices.Size(),
        source.vertexCount,
        &vertexStream,
        1);

    const float simplifyScale = meshopt_simplifyScale(source.positions.Data(), source.vertexCount, sizeof(float) * 3);
    const float localRadius = mesh->GetAABB().IsValid() ? mesh->GetAABB().GetRadius() : 0.0f;

    uint32 options = 0;

    if (settings.lockBorder)
    {
        options |= meshopt_SimplifyLockBorder;
    }

    if (settings.prune)
    {
        options |= meshopt_SimplifyPrune;
    }

    MeshLodGenerationResult result;
    result.sourceDataHash = source.dataHash;

    const uint8 numLods = MathUtil::Clamp(settings.numLods, uint8(1), MaxMeshLods);

    size_t previousIndexCount = source.indices.Size();

    // screen sizes are fractions of screen height, so LOD 0 effectively covers everything up to 1
    float previousScreenSize = 1.0f;

    Array<uint32> simplifiedIndices;
    simplifiedIndices.Resize(source.indices.Size());

    for (uint8 lodIndex = 1; lodIndex < numLods; lodIndex++)
    {
        const float triangleRatio = MathUtil::Clamp(settings.triangleRatios[lodIndex], 0.0f, 1.0f);

        if (triangleRatio <= 0.0f)
        {
            break;
        }

        size_t targetIndexCount = size_t(double(source.indices.Size()) * double(triangleRatio));
        targetIndexCount = (targetIndexCount / 3) * 3;

        if (targetIndexCount < 3)
        {
            break;
        }

        float resultError = 0.0f;

        const size_t numIndices = source.attributeWeights.Any()
            ? meshopt_simplifyWithAttributes(
                  simplifiedIndices.Data(),
                  weldedIndices.Data(),
                  weldedIndices.Size(),
                  source.positions.Data(),
                  source.vertexCount,
                  sizeof(float) * 3,
                  source.attributes.Data(),
                  source.attributeWeights.Size() * sizeof(float),
                  source.attributeWeights.Data(),
                  source.attributeWeights.Size(),
                  nullptr,
                  targetIndexCount,
                  settings.maxRelativeError,
                  options,
                  &resultError)
            : meshopt_simplify(
                  simplifiedIndices.Data(),
                  weldedIndices.Data(),
                  weldedIndices.Size(),
                  source.positions.Data(),
                  source.vertexCount,
                  sizeof(float) * 3,
                  targetIndexCount,
                  settings.maxRelativeError,
                  options,
                  &resultError);

        if (numIndices < 3)
        {
            break;
        }

        // the simplifier can stop short on topology constraints - a LOD that barely differs isn't worth keeping
        if (double(numIndices) > double(previousIndexCount) * 0.95)
        {
            HYP_LOG(Rendering, Info, "Stopping LOD generation at LOD {} - simplification reached its limit at {} triangles",
                lodIndex, numIndices / 3);

            break;
        }

        GeneratedMeshLod generatedLod;
        BuildCompactLod(source, Span<const uint32>(simplifiedIndices.Data(), numIndices), generatedLod);

        generatedLod.desc.geometricError = resultError * simplifyScale;

        // a LOD may switch in once its geometric error projects to no more than the pixel budget allows
        float screenSize = previousScreenSize * 0.5f;

        if (generatedLod.desc.geometricError > MathUtil::epsilonF && localRadius > MathUtil::epsilonF)
        {
            constexpr float referenceScreenHeightPixels = 1080.0f;

            screenSize = 2.0f * localRadius * (settings.maxScreenErrorPixels / referenceScreenHeightPixels) / generatedLod.desc.geometricError;
        }

        generatedLod.desc.screenSize = MathUtil::Clamp(MathUtil::Min(screenSize, previousScreenSize), 0.0f, 1.0f);

        previousScreenSize = generatedLod.desc.screenSize;
        previousIndexCount = numIndices;

        result.lods.PushBack(std::move(generatedLod));
    }

    return result;
}

#else

TResult<MeshLodGenerationResult> MeshLodGenerator::Generate(const Mesh* mesh, const MeshLodGenerationSettings& settings)
{
    return HYP_MAKE_ERROR(Error, "Built without meshoptimizer - mesh LODs cannot be generated");
}

#endif // HYP_MESHOPTIMIZER

Result MeshLodGenerator::Apply(Mesh* mesh, const MeshLodGenerationResult& result)
{
    if (!mesh)
    {
        return HYP_MAKE_ERROR(Error, "No mesh to apply LODs to");
    }

    const uint8 numGeneratedLods = uint8(MathUtil::Min(result.lods.Size(), size_t(MaxMeshLods - 1)));

    for (uint8 lodIndex = 1; lodIndex <= numGeneratedLods; lodIndex++)
    {
        const GeneratedMeshLod& generatedLod = result.lods[lodIndex - 1];

        VertexArrayView vertices {};
        vertices.floatData = generatedLod.vertexData.Data();
        vertices.vertexCount = generatedLod.desc.numVertices;
        vertices.layoutDesc = mesh->GetMeshDesc().meshAttributes.inputLayout;

        const ubyte* indexBytes = reinterpret_cast<const ubyte*>(generatedLod.indices.Data());

        mesh->SetLodData(
            lodIndex,
            generatedLod.desc,
            vertices,
            ConstByteView(indexBytes, indexBytes + generatedLod.indices.ByteSize()));
    }

    if (numGeneratedLods + 1 < MaxMeshLods)
    {
        mesh->ClearLods(numGeneratedLods + 1);
    }

    MeshLodGenerationSettings settings = mesh->GetLodGenerationSettings();
    settings.sourceDataHash = result.sourceDataHash;
    mesh->SetLodGenerationSettings(settings);

    mesh->UploadGpuData();

    return {};
}

} // namespace Hyperion
