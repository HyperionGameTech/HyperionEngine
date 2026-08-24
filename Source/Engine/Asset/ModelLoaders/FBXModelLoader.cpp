/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/ModelLoaders/FBXModelLoader.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Scene/Entity.hpp>
#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/DetachedScene.hpp>
#include <Scene/Node.hpp>

#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/AnimationComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/Loader.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Compression/Archive.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>
#include <Core/Memory/Allocator/SlabAllocator.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Framework/EngineDriver.hpp>

#include <algorithm>
#include <string>

#ifdef HYP_ZLIB
#include <zlib.h>
#endif

#include <FBXModelLoader.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

using FatVertex = TVertex<VT_Simple | VT_Skeletal>;

#pragma region Helpers

static Pair<Array<FatVertex>, Array<uint32>> CalculateIndices(const Array<FatVertex>& vertices)
{
    Map<FatVertex, uint32> indexMap;

    Array<uint32> indices;
    indices.Reserve(vertices.Size());

    /* This will be our resulting buffer with only the vertices we need. */
    Array<FatVertex> newVertices;
    newVertices.Reserve(vertices.Size());

    for (const auto& vertex : vertices)
    {
        /* Check if the vertex already exists in our map */
        auto it = indexMap.Find(vertex);

        /* If it does, push to our indices */
        if (it != indexMap.End())
        {
            indices.PushBack(it->second);

            continue;
        }

        const uint32 meshIndex = uint32(newVertices.Size());

        /* The vertex is unique, so we push it. */
        newVertices.PushBack(vertex);
        indices.PushBack(meshIndex);

        indexMap[vertex] = meshIndex;
    }

    return { std::move(newVertices), std::move(indices) };
}

#pragma endregion Helpers

#pragma region Allocation

class FBXAllocator : public TArena<DynamicAllocator>
{
public:
    static constexpr size_t BlockSize = 4 * 1024 * 1024; // 4 MB

    FBXAllocator()
        : TArena(BlockSize)
    {
    }
};

struct FBXLoaderMemory
{
    FBXAllocator allocator;
    SlabAllocator* objectAllocator;
};

thread_local FBXLoaderMemory* t_fbxMemory = nullptr;

void InitFBXLoaderMemory();
void ShutdownFBXLoaderMemory();

namespace memory {

template <class T, class T2>
struct DefaultAllocatorInstanceHelper;

template <>
struct DefaultAllocatorInstanceHelper<FBXAllocator, void>
{
    HYP_FORCE_INLINE FBXAllocator& operator()() const
    {
        return t_fbxMemory->allocator;
    }
};

} // namespace memory

#pragma endregion Allocation

constexpr char HeaderString[] = "Kaydara FBX Binary  ";
constexpr uint8 HeaderBytes[] = { 0x1A, 0x00 };

using FBXPropertyValue = Variant<int16, int32, int64, uint32, float, double, bool, String, ByteBuffer>;
using FBXObjectID = int64;

struct FBXProperty
{
    static const FBXProperty s_empty;

    FBXPropertyValue value;
    Array<FBXPropertyValue, FBXAllocator> arrayElements;

    explicit operator bool() const
    {
        return value.IsValid()
            || AnyOf(arrayElements, &FBXPropertyValue::IsValid);
    }
};

const FBXProperty FBXProperty::s_empty = {};

struct FBXTemplate
{
    String name;
    int32 count = 0;
    Array<struct FBXDefinitionProperty, FBXAllocator> properties;
};

struct FBXObject
{
    static const FBXObject s_empty;

    String name;
    Array<FBXProperty, FBXAllocator> properties;
    Array<FBXObject*, FBXAllocator> children;
    const FBXTemplate* fbxTemplate = nullptr;

    HYP_FORCE_INLINE const FBXProperty& GetProperty(uint32 index) const
    {
        if (index >= properties.Size())
        {
            return FBXProperty::s_empty;
        }

        return properties[index];
    }

    template <class T>
    bool GetFBXPropertyValue(uint32 index, T& out) const
    {
        out = {};

        if (const FBXProperty& property = GetProperty(index))
        {
            if (const T* ptr = property.value.TryGet<T>())
            {
                out = *ptr;

                return true;
            }
        }

        return false;
    }

    const FBXObject& FindChild(const UTF8StringView& childName) const
    {
        for (FBXObject* child : children)
        {
            if (child == nullptr)
            {
                continue;
            }

            if (child->name == childName)
            {
                return *child;
            }
        }

        return s_empty;
    }

    HYP_FORCE_INLINE FBXObject& operator[](const UTF8StringView& childName)
    {
        return const_cast<FBXObject&>(static_cast<const FBXObject&>(*this)[childName]);
    }

    HYP_FORCE_INLINE const FBXObject& operator[](const UTF8StringView& childName) const
    {
        return FindChild(childName);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return !Empty();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return name.Empty()
            && properties.Empty()
            && children.Empty();
    }

    HYP_FORCE_INLINE bool IsFBXType(UTF8StringView sv) const
    {
        return fbxTemplate != nullptr
            && fbxTemplate->name == sv;
    }
};

struct FBXDefinitionProperty
{
    uint8 type;
    String name;
};

struct FBXConnection
{
    String type;
    FBXObjectID left;
    FBXObjectID right;
    String propertyName; // only valid for "OP" connections
};

struct FBXTexture
{
    String name;
    String filename;
};

struct FBXMaterial
{
    String name;
    Vec4f diffuseColor { 1.0f, 1.0f, 1.0f, 1.0f };
    double transparencyFactor = 0.0;
    Map<String, FBXObjectID, FBXAllocator> textureConnections; // FBX property name (e.g. "DiffuseColor") -> Texture object id
};

// 1 second, expressed in FBX's internal time units
static constexpr int64 FBXTimeUnitsPerSecond = 46186158000LL;

struct FBXAnimCurve
{
    Array<int64, FBXAllocator> keyTimes;
    Array<float, FBXAllocator> keyValues;

    float Evaluate(float timeSeconds, float defaultValue) const
    {
        if (keyTimes.Empty())
        {
            return defaultValue;
        }

        const int64 timeUnits = int64(double(timeSeconds) * double(FBXTimeUnitsPerSecond));

        if (timeUnits <= keyTimes.Front())
        {
            return keyValues.Front();
        }

        if (timeUnits >= keyTimes.Back())
        {
            return keyValues.Back();
        }

        for (size_t index = 1; index < keyTimes.Size(); ++index)
        {
            if (timeUnits <= keyTimes[index])
            {
                const int64 prevTime = keyTimes[index - 1];
                const int64 nextTime = keyTimes[index];

                const float alpha = nextTime != prevTime
                    ? float(double(timeUnits - prevTime) / double(nextTime - prevTime))
                    : 0.0f;

                return MathUtil::Lerp(keyValues[index - 1], keyValues[index], alpha);
            }
        }

        return keyValues.Back();
    }
};

struct FBXAnimCurveNode
{
    Map<String, FBXObjectID> curveIds; // component name (e.g. "d|X") -> AnimationCurve id

    FBXObjectID targetNodeId = 0;
    String targetProperty; // "Lcl Translation", "Lcl Rotation", "Lcl Scaling"
};

struct FBXCluster
{
    String name;
    Mat4f transform;
    Mat4f transformLink;
    Array<int32, FBXAllocator> vertexIndices;
    Array<double, FBXAllocator> boneWeights;

    FBXObjectID limbId = 0;
};

struct FBXSkin
{
    Set<FBXObjectID, FBXAllocator> clusterIds;
};

struct FBXPoseNode
{
    FBXObjectID nodeId = 0;
    Mat4f matrix;
};

struct FBXBindPose
{
    String name;
    Array<FBXPoseNode, FBXAllocator> poseNodes;
};

struct FBXMesh
{
    FBXObject* srcObject = nullptr;

    String name;

    FBXObjectID skinId = 0;

    Array<float> vertexData;
    Array<uint32> indices;

    Map<int32, Array<uint32, FBXAllocator>, FBXAllocator> controlPointToVertices;

    BoundingBox bounds;

    Optional<Handle<Mesh>> result;

    const Handle<Mesh>& GetResultObject()
    {
        if (!result.HasValue())
        {
            Assert(name.Length() > 0);

            VertexArrayView vertexArrayView {};
            vertexArrayView.floatData = vertexData.Data();
            vertexArrayView.layoutDesc = VertexInputLayoutDesc { VT_Simple | VT_Skeletal };
            vertexArrayView.vertexCount = vertexData.ByteSize() / vertexArrayView.layoutDesc.VertexSize();

            MeshDesc meshDesc;
            meshDesc.meshAttributes.inputLayout = vertexArrayView.layoutDesc;
            meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
            meshDesc.meshAttributes.topology = TOP_TRIANGLES;
            meshDesc.lods[0].numVertices = uint32(vertexArrayView.vertexCount);
            meshDesc.lods[0].numIndices = uint32(indices.Size());

            MeshDataView data {};
            data.vertices[0] = vertexArrayView;
            data.indices[0] = indices.ToByteView();

            Handle<Mesh> mesh = MakeHandle<Mesh>();
            mesh->SetName(CreateNameFromDynamicString(name));
            mesh->SetMeshData(meshDesc, data);

            bounds = mesh->GetAABB();

            result.Set(std::move(mesh));
        }

        return result.Get();
    }
};

struct FBXNode
{
    FBXObject* srcObject = nullptr;

    String name;

    FBXObjectID parentId = 0;
    FBXObjectID skeletonHolderNodeId = 0;

    FBXObjectID meshId = 0;
    FBXObjectID materialId = 0;

    Set<FBXObjectID, FBXAllocator> childIds;

    Transform localTransform;

    Mat4f worldBindMatrix;
    Mat4f localBindMatrix;

    Optional<Handle<Skeleton>> skeleton;
};

struct FBXNodeMapping
{
    FBXObject* object;

    Variant<
        FBXMesh,
        FBXNode,
        FBXCluster,
        FBXSkin,
        FBXBindPose,
        FBXMaterial,
        FBXTexture,
        FBXAnimCurve,
        FBXAnimCurveNode>
        data;
};

const FBXObject FBXObject::s_empty;

using FBXVersion = uint32;

static void DeleteFBXObjectsRecursively(FBXObject* object)
{
    if (!object)
    {
        return;
    }

    for (FBXObject* child : object->children)
    {
        DeleteFBXObjectsRecursively(child);
    }

    object->~FBXObject();
    t_fbxMemory->objectAllocator->Free(object);
}

static Result ReadFBXProperty(ByteReader& reader, FBXProperty& outProperty);
static Result ReadFBXPropertyValue(ByteReader& reader, uint8 type, FBXPropertyValue& outValue);
static uint64 ReadFBXOffset(ByteReader& reader, FBXVersion version);
static Result ReadFBXNode(ByteReader& reader, FBXVersion version, FBXObject*& out);
static uint8 PrimitiveSize(uint8 primitiveType);
static bool ReadMagic(ByteReader& reader);

static bool ReadMagic(ByteReader& reader)
{
    if (reader.Max() < sizeof(HeaderString) + sizeof(HeaderBytes))
    {
        return false;
    }

    char readMagic[sizeof(HeaderString)] = { '\0' };

    reader.Read(readMagic, sizeof(HeaderString));

    if (Memory::StrCmp(readMagic, HeaderString, sizeof(HeaderString)) != 0)
    {
        return false;
    }

    uint8 bytes[sizeof(HeaderBytes)];
    reader.Read(bytes, sizeof(HeaderBytes));

    if (Memory::Compare(bytes, HeaderBytes, sizeof(HeaderBytes)) != 0)
    {
        return false;
    }

    return true;
}

static Result ReadFBXPropertyValue(ByteReader& reader, uint8 type, FBXPropertyValue& outValue)
{
    switch (type)
    {
    case 'R':
    case 'S':
    {
        uint32 length;
        reader.Read(&length);

        ByteBuffer byteBuffer = reader.Read(length);

        if (type == 'R')
        {
            outValue.Set(std::move(byteBuffer));
        }
        else
        {
            outValue.Set(String(byteBuffer.ToByteView()));
        }

        return {};
    }
    case 'Y':
    {
        // int16 (signed)
        int16 i;
        reader.Read(&i);

        outValue.Set(i);

        return {};
    }
    case 'I':
    {
        // int32 (signed)
        int32 i;
        reader.Read(&i);

        outValue.Set(i);

        return {};
    }
    case 'L':
    {
        // int64 (Signed)
        int64 i;
        reader.Read(&i);

        outValue.Set(i);

        return {};
    }
    case 'C': // fallthrough
    case 'B':
    {
        uint8 b;
        reader.Read(&b);

        outValue.Set(bool(b != 0));

        return {};
    }
    case 'F':
    {
        // float
        float f;
        reader.Read(&f);

        outValue.Set(f);

        return {};
    }
    case 'D':
    {
        // double
        double d;
        reader.Read(&d);

        outValue.Set(d);

        return {};
    }
    default:
        return HYP_MAKE_ERROR(Error, "Invalid property type '{}'", int(type));
    }
}

static uint8 PrimitiveSize(uint8 primitiveType)
{
    switch (primitiveType)
    {
    case 'Y':
        return 2;
    case 'C':
    case 'B': // fallthrough
        return 1;
    case 'I':
        return 4;
    case 'F':
        return 4;
    case 'D':
        return 8;
    case 'L':
        return 8;
    default:
        return 0;
    }
}

static Result ReadFBXProperty(ByteReader& reader, FBXProperty& outProperty)
{
    uint8 type;
    reader.Read(&type);

    if (type < 'Z')
    {
        Result result = ReadFBXPropertyValue(reader, type, outProperty.value);

        if (!result)
        {
            return result;
        }
    }
    else
    {
        // array -- can be zlib compressed

        const uint8 arrayHeldType = type - ('a' - 'A');

        uint32 numElements;
        reader.Read(&numElements);

        uint32 encoding;
        reader.Read(&encoding);

        uint32 length;
        reader.Read(&length);

        if (encoding != 0)
        {
            if (!Archive::IsEnabled())
            {
                return HYP_MAKE_ERROR(Error, "FBX array property compression requested, but Archive is not enabled");
            }

            ByteBuffer compressedBuffer = reader.Read(length);

            unsigned long compressedSize(length);
            unsigned long decompressedSize(PrimitiveSize(arrayHeldType) * numElements);

            Archive archive(std::move(compressedBuffer), decompressedSize);

            ByteBuffer decompressedBuffer;
            if (Result decompressResult = archive.Decompress(decompressedBuffer); decompressResult.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to decompress FBX array property: {}", decompressResult.GetError().GetMessage());
            }

            MemoryByteReader mbr { decompressedBuffer.ToByteView() };

            for (uint32 index = 0; index < numElements; ++index)
            {
                FBXPropertyValue value;

                Result result = ReadFBXPropertyValue(mbr, arrayHeldType, value);

                if (!result)
                {
                    return result;
                }

                outProperty.arrayElements.PushBack(std::move(value));
            }
        }
        else
        {
            for (uint32 index = 0; index < numElements; ++index)
            {
                FBXPropertyValue value;

                Result result = ReadFBXPropertyValue(reader, arrayHeldType, value);

                if (!result)
                {
                    return result;
                }

                outProperty.arrayElements.PushBack(std::move(value));
            }
        }
    }

    return {};
}

static uint64 ReadFBXOffset(ByteReader& reader, FBXVersion version)
{
    if (version >= 7500)
    {
        uint64 value;
        reader.Read(&value);

        return value;
    }
    else
    {
        uint32 value;
        reader.Read(&value);

        return uint64(value);
    }
}

static Result ReadFBXNode(ByteReader& reader, FBXVersion version, FBXObject*& out)
{
    uint64 endPosition = ReadFBXOffset(reader, version);
    uint64 numProperties = ReadFBXOffset(reader, version);
    uint64 propertyListLength = ReadFBXOffset(reader, version);

    uint8 nameLength;
    reader.Read(&nameLength);

    if (endPosition == 0 && numProperties == 0 && propertyListLength == 0 && nameLength == 0)
    {
        // Null record terminating the parent's list of children -- not an actual node
        out = nullptr;

        return {};
    }

    out = (FBXObject*)t_fbxMemory->objectAllocator->Allocate();
    new (out) FBXObject();

    ByteBuffer nameBuffer = reader.Read(nameLength);

    out->name = String(nameBuffer.ToByteView());

    for (uint32 index = 0; index < numProperties; ++index)
    {
        FBXProperty property;

        Result result = ReadFBXProperty(reader, property);

        if (!result)
        {
            return result;
        }

        if (property)
        {
            out->properties.PushBack(std::move(property));
        }
    }

    while (reader.Position() < endPosition)
    {
        FBXObject* childObject = nullptr;
        Result result = ReadFBXNode(reader, version, childObject);

        if (!result)
        {
            return result;
        }

        if (childObject != nullptr)
        {
            out->children.PushBack(childObject);
        }
    }

    return {};
}

enum FBXVertexMappingType
{
    INVALID_MAPPING_TYPE = 0,
    BY_POLYGON_VERTEX,
    BY_POLYGON,
    BY_VERTEX
};

static FBXVertexMappingType GetVertexMappingType(const FBXObject& object)
{
    FBXVertexMappingType mappingType = INVALID_MAPPING_TYPE;

    if (object)
    {
        String mappingTypeStr;

        if (object.GetFBXPropertyValue<String>(0, mappingTypeStr))
        {
            if (mappingTypeStr == "ByPolygonVertex")
            {
                mappingType = BY_POLYGON_VERTEX;
            }
            else if (mappingTypeStr == "ByPolygon")
            {
                mappingType = BY_POLYGON;
            }
            else if (mappingTypeStr.StartsWith("ByVert"))
            {
                mappingType = BY_VERTEX;
            }
        }
    }

    return mappingType;
}

enum FBXReferenceType
{
    REFERENCE_DIRECT,
    REFERENCE_INDEX_TO_DIRECT
};

static FBXReferenceType GetReferenceType(const FBXObject& object)
{
    String referenceTypeStr;

    if (object.GetFBXPropertyValue<String>(0, referenceTypeStr))
    {
        if (referenceTypeStr == "IndexToDirect" || referenceTypeStr == "Index")
        {
            return REFERENCE_INDEX_TO_DIRECT;
        }
    }

    return REFERENCE_DIRECT;
}

template <class T, class AllocatorType>
static Result ReadBinaryArray(const FBXObject& object, Array<T, AllocatorType>& ary)
{
    if (FBXProperty property = object.GetProperty(0))
    {
        ary.Resize(property.arrayElements.Size());

        for (size_t index = 0; index < property.arrayElements.Size(); ++index)
        {
            const auto& item = property.arrayElements[index];

            if (const T* pValue = item.TryGet<T>())
            {
                ary[index] = *pValue;
            }
            else
            {
                return HYP_MAKE_ERROR(Error, "Type mismatch for array data");
            }
        }
    }

    return {};
}

static Result ReadIndexedLayer(
    const FBXObject& layerElementNode,
    const char* dataChildName,
    const char* indexChildName,
    uint32 numComponents,
    size_t numPolygonVertices,
    Array<double>& outFlat)
{
    outFlat.Clear();

    if (!layerElementNode)
    {
        return {};
    }

    const FBXVertexMappingType mappingType = GetVertexMappingType(layerElementNode["MappingInformationType"]);

    if (mappingType != BY_POLYGON_VERTEX)
    {
        HYP_LOG(Assets, Warning, "FBX layer element mapping type is not supported (only ByPolygonVertex is handled), skipping layer");

        return {};
    }

    const FBXReferenceType referenceType = GetReferenceType(layerElementNode["ReferenceInformationType"]);

    Array<double> rawData;

    if (const FBXObject& dataNode = layerElementNode[dataChildName])
    {
        Result result = ReadBinaryArray<double>(dataNode, rawData);

        if (!result)
        {
            return result;
        }
    }
    else
    {
        return {};
    }

    if (rawData.Empty() || numComponents == 0 || rawData.Size() % numComponents != 0)
    {
        HYP_LOG(Assets, Warning, "FBX layer element data has an invalid element count, skipping layer");

        return {};
    }

    if (referenceType == REFERENCE_INDEX_TO_DIRECT)
    {
        Array<int32> indices;

        if (const FBXObject& indexNode = layerElementNode[indexChildName])
        {
            Result result = ReadBinaryArray<int32>(indexNode, indices);

            if (!result)
            {
                return result;
            }
        }

        if (indices.Size() != numPolygonVertices)
        {
            HYP_LOG(Assets, Warning, "FBX layer element index count ({}) does not match polygon-vertex count ({}), skipping layer",
                    indices.Size(), numPolygonVertices);

            return {};
        }

        const size_t numDirectElements = rawData.Size() / numComponents;

        outFlat.Resize(numPolygonVertices * numComponents);

        for (size_t index = 0; index < numPolygonVertices; ++index)
        {
            int32 elementIndex = indices[index];

            if (elementIndex < 0)
            {
                elementIndex = (elementIndex * -1) - 1;
            }

            if (size_t(elementIndex) >= numDirectElements)
            {
                HYP_LOG(Assets, Warning, "FBX layer element index {} out of range ({} elements), skipping layer", elementIndex, numDirectElements);

                outFlat.Clear();

                return {};
            }

            for (uint32 component = 0; component < numComponents; ++component)
            {
                outFlat[index * numComponents + component] = rawData[size_t(elementIndex) * numComponents + component];
            }
        }
    }
    else
    {
        if (rawData.Size() != numPolygonVertices * numComponents)
        {
            HYP_LOG(Assets, Warning, "FBX layer element data count ({}) does not match polygon-vertex count ({}) for direct reference, skipping layer",
                    rawData.Size(), numPolygonVertices);

            return {};
        }

        outFlat = std::move(rawData);
    }

    return {};
}

template <class T>
static bool GetFBXObjectInMapping(Map<FBXObjectID, FBXNodeMapping, FBXAllocator>& mapping, FBXObjectID id, T*& out)
{
    out = nullptr;

    const auto it = mapping.Find(id);

    if (it == mapping.End())
    {
        return false;
    }

    if (auto* ptr = it->second.data.TryGet<T>())
    {
        out = ptr;

        return true;
    }

    return false;
}

struct UniqueNameGenerator
{
    const String& baseName;
    Set<String, FBXAllocator> used;

    HYP_NODISCARD String operator()(const UTF8StringView& nodeName)
    {
        const String nodeNameWithBaseName = baseName + "_" + nodeName;

        String name = nodeNameWithBaseName;
        int counter = 0;

        while (used.Contains(name))
        {
            name = HYP_FORMAT("{}_{}", nodeNameWithBaseName, ++counter);
        }

        used.Insert(name);

        return name;
    }
};

FBXModelLoader::FBXModelLoader() = default;

AssetLoadResult FBXModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    // Include our root dir as part of the path
    const FilePath path = state.filepath;
    const FilePath currentDir = FilePath::Current();
    const FilePath basePath = path.BasePath();

    const String baseName = StringUtil::StripExtension(path.Basename());

    Handle<Node> top = MakeHandle<Node>(CreateNameFromDynamicString(baseName));
    Handle<Skeleton> rootSkeleton = MakeHandle<Skeleton>();

    FileByteReader fbr { path };

    if (fbr.Eof())
    {
        return HYP_MAKE_ERROR(AssetLoadError, "File not open", AssetLoadError::ERR_EOF);
    }

    if (!ReadMagic(fbr))
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Invalid magic header");
    }

    InitFBXLoaderMemory();
    HYP_DEFER({ ShutdownFBXLoaderMemory(); });

    FBXVersion version;
    fbr.Read(&version, sizeof(FBXVersion));

    FBXObject root;

    FBXNode rootFbxNode;
    rootFbxNode.name = "[FBX Model Root]";
    rootFbxNode.srcObject = &root;

    HYP_DEFER({
        for (FBXObject* child : root.children)
        {
            DeleteFBXObjectsRecursively(child);
        }
    });

    UniqueNameGenerator getUniqueMeshName { baseName };
    UniqueNameGenerator getUniqueMaterialName { baseName };

    do
    {
        FBXObject* object = nullptr;

        Result result = ReadFBXNode(fbr, version, object);

        if (!result)
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read FBX node: {}", 0, result.GetError().GetMessage());
        }

        if (object == nullptr || object->Empty())
        {
            break;
        }

        root.children.PushBack(object);
    }
    while (true);

    Array<FBXTemplate> objectTemplates;

    if (const auto& definitionsNode = root["Definitions"])
    {
        int32 numDefinitions = 0;

        if (const auto& countNode = definitionsNode["Count"])
        {
            countNode.GetFBXPropertyValue<int32>(0, numDefinitions);
        }

        for (auto& child : definitionsNode.children)
        {
            if (child->name == "ObjectType")
            {
                FBXTemplate& objectTemplate = objectTemplates.EmplaceBack();
                objectTemplate.name = String::empty;
                objectTemplate.count = 0;

                child->GetFBXPropertyValue(0, objectTemplate.name);
                for (auto& templateChild : child->children)
                {
                    if (templateChild->name == "Count")
                    {
                        templateChild->GetFBXPropertyValue(0, objectTemplate.count);
                    }
                    else if (templateChild->name == "PropertyTemplate")
                    {
                    }
                }
            }
        }
    }

    const auto getTemplate = [&objectTemplates](UTF8StringView name) -> const FBXTemplate*
    {
        for (const FBXTemplate& tmpl : objectTemplates)
        {
            if (tmpl.name == name)
            {
                return &tmpl;
            }
        }

        return nullptr;
    };

    const auto readMatrix = [](const FBXObject& object) -> Mat4f
    {
        if (!object)
        {
            return Mat4f::zeros;
        }

        Mat4f matrix = Mat4f::zeros;

        if (FBXProperty matrixProperty = object.GetProperty(0))
        {
            if (matrixProperty.arrayElements.Size() == 16)
            {
                for (size_t index = 0; index < matrixProperty.arrayElements.Size(); ++index)
                {
                    const auto& item = matrixProperty.arrayElements[index];

                    if (const auto* valueDouble = item.TryGet<double>())
                    {
                        matrix.values[index] = float(*valueDouble);
                    }
                    else if (const auto* valueFloat = item.TryGet<float>())
                    {
                        matrix.values[index] = *valueFloat;
                    }
                }
            }
            else
            {
                HYP_LOG(Assets, Warning, "Invalid matrix in FBX fbx_node -- invalid size");
            }
        }

        return matrix;
    };

    const auto readVec3Property = [](const FBXObject& object) -> Vec3f
    {
        double x, y, z;

        object.GetFBXPropertyValue<double>(4, x);
        object.GetFBXPropertyValue<double>(5, y);
        object.GetFBXPropertyValue<double>(6, z);

        return { float(x), float(y), float(z) };
    };

    Map<FBXObjectID, FBXNodeMapping, FBXAllocator> objectMapping;
    Set<FBXObjectID, FBXAllocator> bindPoseIds;
    Array<FBXConnection, FBXAllocator> connections;

    const auto getFbxObject = [&objectMapping](FBXObjectID id, auto*& out)
    {
        return GetFBXObjectInMapping(objectMapping, id, out);
    };

    const auto getSkeletonFromLimbNode = [&](const FBXNode& limbNode) -> Handle<Skeleton>
    {
        if (!limbNode.skeletonHolderNodeId)
        {
            return Handle<Skeleton>::Null();
        }

        FBXNode* skeletonHolderNode;

        if (getFbxObject(limbNode.skeletonHolderNodeId, skeletonHolderNode))
        {
            if (skeletonHolderNode->skeleton.HasValue())
            {
                return skeletonHolderNode->skeleton.Get();
            }
        }

        return Handle<Skeleton>::Null();
    };

    const auto applyClustersToMesh = [&](FBXMesh& mesh) -> Handle<Skeleton>
    {
        if (!mesh.skinId)
        {
            return Handle<Skeleton>::Null();
        }

        FBXSkin* skin;

        if (!getFbxObject(mesh.skinId, skin))
        {
            return Handle<Skeleton>::Null();
        }

        Handle<Skeleton> skeleton = rootSkeleton;

        for (const FBXObjectID clusterId : skin->clusterIds)
        {
            FBXCluster* cluster;

            if (!getFbxObject(clusterId, cluster))
            {
                HYP_LOG(Assets, Warning, "Cluster with id {} not found in mapping!", clusterId);

                continue;
            }

            if (cluster->limbId)
            {
                FBXNode* limbNode;

                if (!getFbxObject(cluster->limbId, limbNode))
                {
                    HYP_LOG(Assets, Warning, "LimbNode with id {} not found in mapping!", cluster->limbId);

                    continue;
                }

#if 0
                Handle<Skeleton> skeletonFromLimb = getSkeletonFromLimbNode(*limbNode);

                if (skeleton && skeletonFromLimb && skeleton != skeletonFromLimb) {
                    HYP_LOG(Assets, Warning, "LimbNode with id {} has Skeleton with id {}, but multiple skeletons are attached to the mesh!",
                        cluster->limbId,
                        skeletonFromLimb->Id());
                }

                skeleton = skeletonFromLimb;

                if (!skeleton) {
                    HYP_LOG(Assets, Warning, "LimbNode with id {} has no Skeleton!", cluster->limbId);

                    continue;
                }
#endif

                const uint32 boneIndex = skeleton->FindBoneIndex(limbNode->name);

                if (boneIndex == uint32(-1))
                {
                    HYP_LOG(Assets, Warning, "LimbNode with id {}: Bone with name {} not found in Skeleton",
                            cluster->limbId, limbNode->name);

                    continue;
                }

                const size_t numVertices = mesh.vertexData.ByteSize() / sizeof(FatVertex);
                FatVertex* vertices = reinterpret_cast<FatVertex*>(mesh.vertexData.Data());

                for (size_t index = 0; index < cluster->vertexIndices.Size(); ++index)
                {
                    if (index >= cluster->boneWeights.Size())
                    {
                        HYP_LOG(Assets, Warning, "Index {} out of range of bone weights", index);

                        break;
                    }

                    int32 controlPointIndex = cluster->vertexIndices[index];

                    if (controlPointIndex < 0)
                    {
                        controlPointIndex = (controlPointIndex * -1) - 1;
                    }

                    const float weight = float(cluster->boneWeights[index]);

                    const auto controlPointIt = mesh.controlPointToVertices.Find(controlPointIndex);

                    if (controlPointIt == mesh.controlPointToVertices.End())
                    {
                        HYP_LOG(Assets, Warning, "FBX control point index {} has no corresponding mesh vertices", controlPointIndex);

                        continue;
                    }

                    for (const uint32 expandedVertexIndex : controlPointIt->second)
                    {
                        if (expandedVertexIndex >= numVertices)
                        {
                            HYP_LOG(Assets, Warning, "Expanded vertex index {} out of range of vertex count {}", expandedVertexIndex, numVertices);

                            continue;
                        }

                        FatVertex& vertex = vertices[expandedVertexIndex];

                        const uint32 boneSlot = vertex.NumBoneIndices();

                        if (boneSlot >= MAX_BONE_INDICES)
                        {
                            HYP_LOG(Assets, Warning, "Vertex {} already has the maximum number ({}) of bone influences, skipping additional influence",
                                    expandedVertexIndex, MAX_BONE_INDICES);

                            continue;
                        }

                        vertex.SetBoneIndex(boneSlot, uint8(boneIndex));
                        vertex.SetBoneWeight(boneSlot, weight);
                    }
                }
            }
        }

        return skeleton;
    };

    if (const FBXObject& connectionsNode = root["Connections"])
    {
        for (auto& child : connectionsNode.children)
        {
            FBXConnection connection {};

            child->GetFBXPropertyValue<String>(0, connection.type);

            if (!child->GetFBXPropertyValue<FBXObjectID>(1, connection.left))
            {
                HYP_LOG(Assets, Warning, "Invalid FBX Node connection, cannot get left id value");
                continue;
            }

            if (!child->GetFBXPropertyValue<FBXObjectID>(2, connection.right))
            {
                HYP_LOG(Assets, Warning, "Invalid FBX Node connection, cannot get right id value");
                continue;
            }

            if (connection.type == "OP")
            {
                child->GetFBXPropertyValue<String>(3, connection.propertyName);
            }

            connections.PushBack(connection);
        }
    }

    if (FBXObject& objectsNode = root["Objects"])
    {
        for (FBXObject* childObject : objectsNode.children)
        {
            // Get the FBXTemplate for this object, if it exists
            const FBXTemplate* fbxTemplate = getTemplate(childObject->name);
            if (!fbxTemplate)
            {
                HYP_LOG(Assets, Warning, "No template found for FBX object '{}'", childObject->name);
                continue;
            }

            FBXObjectID objectId;
            childObject->GetFBXPropertyValue<FBXObjectID>(0, objectId);

            String nodeName;
            childObject->GetFBXPropertyValue<String>(1, nodeName);

            childObject->fbxTemplate = fbxTemplate;

            FBXNodeMapping mapping { childObject };

            if (fbxTemplate->name == "Pose")
            {
                String poseType;
                childObject->GetFBXPropertyValue(2, poseType);

                if (poseType == "BindPose")
                {
                    FBXBindPose bindPose;

                    const size_t substrIndex = nodeName.FindFirstIndex("Pose::");

                    if (substrIndex != String::NotFound)
                    {
                        bindPose.name = nodeName.Substr(substrIndex);
                    }
                    else
                    {
                        bindPose.name = nodeName;
                    }

                    for (FBXObject* poseChild : childObject->children)
                    {
                        if (poseChild->name == "PoseNode")
                        {
                            FBXPoseNode pose;

                            if (const FBXObject& nodeNode = poseChild->FindChild("Node"))
                            {
                                nodeNode.GetFBXPropertyValue<FBXObjectID>(0, pose.nodeId);
                            }

                            pose.matrix = readMatrix(poseChild->FindChild("Matrix"));

                            bindPose.poseNodes.PushBack(pose);
                        }
                    }

                    mapping.data.Set(std::move(bindPose));

                    bindPoseIds.Insert(objectId);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "Unsure how to handle Pose type {}", poseType.Data());

                    continue;
                }
            }
            else if (fbxTemplate->name == "Deformer")
            {
                String deformerType;
                childObject->GetFBXPropertyValue(2, deformerType);

                if (deformerType == "Skin")
                {
                    mapping.data.Set(FBXSkin {});
                }
                else if (deformerType == "Cluster")
                {
                    FBXCluster cluster;

                    const auto nameSplit = nodeName.Split(':');

                    if (nameSplit.Size() > 1)
                    {
                        cluster.name = nameSplit[1];
                    }

                    if (const FBXObject& transformChild = childObject->FindChild("Transform"))
                    {
                        cluster.transform = readMatrix(transformChild);
                    }

                    if (const FBXObject& transformLinkChild = childObject->FindChild("TransformLink"))
                    {
                        cluster.transformLink = readMatrix(transformLinkChild);
                    }

                    if (const FBXObject& indicesChild = childObject->FindChild("Indexes"))
                    {
                        Result result = ReadBinaryArray<int32>(indicesChild, cluster.vertexIndices);

                        if (!result)
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read bone indices: {}", result.GetError().GetMessage());
                        }
                    }

                    if (const FBXObject& weightsChild = childObject->FindChild("Weights"))
                    {
                        Result result = ReadBinaryArray<double>(weightsChild, cluster.boneWeights);

                        if (!result)
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read bone weights: {}", result.GetError().GetMessage());
                        }
                    }

                    mapping.data.Set(cluster);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "Unsure how to handle Deformer type {}", deformerType.Data());

                    continue;
                }
            }
            else if (fbxTemplate->name == "Geometry")
            {
                Array<Vec3f> modelVertices;
                Array<uint32> modelIndices;

                Bitset polygonVertexEnds;

                Array<String> layerNodeNames;

                if (const FBXObject& layerNode = (*childObject)["Layer"])
                {
                    for (const auto& layerNodeChild : layerNode.children)
                    {
                        if (layerNodeChild->name == "LayerElement")
                        {
                            if (const auto& layerNodeType = (*layerNodeChild)["Type"])
                            {
                                String layerNodeTypeName;

                                if (layerNodeType.GetFBXPropertyValue(0, layerNodeTypeName))
                                {
                                    layerNodeNames.PushBack(std::move(layerNodeTypeName));
                                }
                            }
                        }
                    }
                }

                if (const FBXObject& verticesNode = (*childObject)["Vertices"])
                {
                    const FBXProperty& verticesProperty = verticesNode.GetProperty(0);
                    const size_t count = verticesProperty.arrayElements.Size();

                    if (count == 0)
                    {
                        continue;
                    }

                    if (count % 3 != 0)
                    {
                        return HYP_MAKE_ERROR(AssetLoadError, "Not a valid triangle mesh");
                    }

                    const size_t numVertices = count / 3;

                    modelVertices.Resize(numVertices);

                    //// \todo Optimize me - dont use variant for vertex data + check each time... should use memcpy or similar
                    for (size_t index = 0; index < numVertices; ++index)
                    {
                        Vec3f position;

                        for (uint32 subIndex = 0; subIndex < 3; ++subIndex)
                        {
                            const FBXPropertyValue& elem = verticesProperty.arrayElements[(index * 3) + size_t(subIndex)];

                            union
                            {
                                float floatValue;
                                double doubleValue;
                            };

                            if (elem.Get<float>(&floatValue))
                            {
                                position[subIndex] = floatValue;
                            }
                            else if (elem.Get<double>(&doubleValue))
                            {
                                position[subIndex] = float(doubleValue);
                            }
                            else
                            {
                                return HYP_MAKE_ERROR(AssetLoadError, "Invalid type for vertex position element -- must be float or double");
                            }
                        }

                        modelVertices[index] = position;
                    }
                }

                if (const FBXObject& indicesNode = (*childObject)["PolygonVertexIndex"])
                {
                    const FBXProperty& indicesProperty = indicesNode.GetProperty(0);
                    const size_t count = indicesProperty.arrayElements.Size();

                    if (count == 0)
                    {
                        continue;
                    }

                    modelIndices.Resize(count);
                    polygonVertexEnds.SetNumBits(count);

                    for (size_t index = 0; index < count; ++index)
                    {
                        int32 i = 0;

                        if (!indicesProperty.arrayElements[index].Get<int32>(&i))
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Invalid index value");
                        }

                        polygonVertexEnds.Set(index, false);

                        if (i < 0)
                        {
                            // Negative indices mark the last vertex of a polygon
                            polygonVertexEnds.Set(index, true);

                            i = (i * -1) - 1;
                        }

                        if (size_t(i) >= modelVertices.Size())
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Index out of range");
                        }

                        modelIndices[index] = uint32(i);
                    }
                }

                // Fan-triangulate polygons, keeping track of the source polygon-vertex for each expanded corner
                Array<uint32> triangulatedControlPoints;
                Array<uint32> triangulatedSourceVertices;

                {
                    size_t polygonStart = 0;

                    for (size_t index = 0; index < modelIndices.Size(); ++index)
                    {
                        if (!polygonVertexEnds.Test(index))
                        {
                            continue;
                        }

                        const size_t polygonFirst = polygonStart;
                        const size_t polygonSize = (index - polygonFirst) + 1;

                        polygonStart = index + 1;

                        if (polygonSize < 3)
                        {
                            HYP_LOG(Assets, Warning, "Skipping degenerate FBX polygon with {} vertices", polygonSize);

                            continue;
                        }

                        for (size_t corner = 1; corner + 1 < polygonSize; ++corner)
                        {
                            const uint32 sourceIndices[3] = {
                                uint32(polygonFirst),
                                uint32(polygonFirst + corner),
                                uint32(polygonFirst + corner + 1)
                            };

                            for (const uint32 sourceIndex : sourceIndices)
                            {
                                triangulatedControlPoints.PushBack(modelIndices[sourceIndex]);
                                triangulatedSourceVertices.PushBack(sourceIndex);
                            }
                        }
                    }
                }

                Array<FatVertex> verticesUnpacked;
                verticesUnpacked.Resize(triangulatedControlPoints.Size());

                for (size_t index = 0; index < triangulatedControlPoints.Size(); ++index)
                {
                    verticesUnpacked[index].SetPosition(modelVertices[triangulatedControlPoints[index]]);
                }

                for (const String& name : layerNodeNames)
                {
                    if (name == "LayerElementUV")
                    {
                        Array<double> uvFlat;
                        Result result = ReadIndexedLayer((*childObject)[name], "UV", "UVIndex", 2, modelIndices.Size(), uvFlat);

                        if (!result)
                        {
                            HYP_LOG(Assets, Warning, "Failed to read FBX UV layer: {}", result.GetError().GetMessage());
                        }

                        for (size_t index = 0; index < triangulatedSourceVertices.Size(); ++index)
                        {
                            const size_t sourceIndex = triangulatedSourceVertices[index];

                            if (sourceIndex * 2 + 1 >= uvFlat.Size())
                            {
                                break;
                            }

                            Vec2f uv { float(uvFlat[sourceIndex * 2 + 0]), float(uvFlat[sourceIndex * 2 + 1]) };

                            verticesUnpacked[index].SetUV0(uv);
                        }
                    }
                    else if (name == "LayerElementNormal")
                    {
                        Array<double> normalFlat;
                        Result result = ReadIndexedLayer((*childObject)[name], "Normals", "NormalsIndex", 3, modelIndices.Size(), normalFlat);

                        if (!result)
                        {
                            HYP_LOG(Assets, Warning, "Failed to read FBX normal layer: {}", result.GetError().GetMessage());
                        }

                        for (size_t index = 0; index < triangulatedSourceVertices.Size(); ++index)
                        {
                            const size_t sourceIndex = triangulatedSourceVertices[index];

                            if (sourceIndex * 3 + 2 >= normalFlat.Size())
                            {
                                break;
                            }

                            Vec3f normal { float(normalFlat[sourceIndex * 3 + 0]), float(normalFlat[sourceIndex * 3 + 1]), float(normalFlat[sourceIndex * 3 + 2]) };

                            verticesUnpacked[index].SetNormal(normal);
                        }
                    }
                }

                auto newVertsAndIndices = CalculateIndices(verticesUnpacked);

                FBXMesh fbxMesh;
                fbxMesh.srcObject = childObject;
                fbxMesh.name = getUniqueMeshName(nodeName);
                fbxMesh.vertexData = Array<float>(reinterpret_cast<const float*>(newVertsAndIndices.first.Data()), newVertsAndIndices.first.ByteSize() / sizeof(float));

                for (size_t index = 0; index < triangulatedControlPoints.Size(); ++index)
                {
                    const int32 controlPointIndex = int32(triangulatedControlPoints[index]);
                    const uint32 expandedVertexIndex = newVertsAndIndices.second[index];

                    Array<uint32, FBXAllocator>& expandedIndices = fbxMesh.controlPointToVertices[controlPointIndex];

                    if (!expandedIndices.Contains(expandedVertexIndex))
                    {
                        expandedIndices.PushBack(expandedVertexIndex);
                    }
                }

                fbxMesh.indices = std::move(newVertsAndIndices.second);

                // Fix coordinate space
                for (size_t index = 0; index + 2 < fbxMesh.indices.Size(); index += 3)
                {
                    std::swap(fbxMesh.indices[index + 1], fbxMesh.indices[index + 2]);
                }

                mapping.data.Set(std::move(fbxMesh));
            }
            else if (fbxTemplate->name == "Model")
            {
                String modelType;
                childObject->GetFBXPropertyValue<String>(2, modelType);

                Transform transform;

                for (FBXObject* modelChild : childObject->children)
                {
                    if (modelChild->name.StartsWith("Properties"))
                    {
                        for (FBXObject* propertiesChild : modelChild->children)
                        {
                            String propertiesChildName;

                            if (!propertiesChild->GetFBXPropertyValue<String>(0, propertiesChildName))
                            {
                                continue;
                            }

                            if (propertiesChildName == "Lcl Translation")
                            {
                                transform.SetTranslation(readVec3Property(*propertiesChild));
                            }
                            else if (propertiesChildName == "Lcl Scaling")
                            {
                                transform.SetScale(readVec3Property(*propertiesChild));
                            }
                            else if (propertiesChildName == "Lcl Rotation")
                            {
                                Vec3f degrees = readVec3Property(*propertiesChild);
                                Vec3f radians;
                                radians.x = MathUtil::DegToRad(degrees.x);
                                radians.y = MathUtil::DegToRad(degrees.y);
                                radians.z = MathUtil::DegToRad(degrees.z);
                                transform.SetRotation(Quat4f(radians).Inverse());
                            }
                        }
                    }
                }

                FBXNode fbxNode;
                fbxNode.srcObject = childObject;
                fbxNode.name = nodeName;
                fbxNode.localTransform = transform;

                mapping.data.Set(fbxNode);
            }
            else if (fbxTemplate->name == "Material")
            {
                FBXMaterial material;
                material.name = getUniqueMaterialName(nodeName);

                for (FBXObject* materialChild : childObject->children)
                {
                    if (!materialChild->name.StartsWith("Properties"))
                    {
                        continue;
                    }

                    for (FBXObject* propertiesChild : materialChild->children)
                    {
                        String propertiesChildName;

                        if (!propertiesChild->GetFBXPropertyValue<String>(0, propertiesChildName))
                        {
                            continue;
                        }

                        if (propertiesChildName == "DiffuseColor")
                        {
                            const Vec3f color = readVec3Property(*propertiesChild);
                            material.diffuseColor = Vec4f(color.x, color.y, color.z, material.diffuseColor.w);
                        }
                        else if (propertiesChildName == "TransparencyFactor" || propertiesChildName == "Opacity")
                        {
                            double transparencyFactor;

                            if (propertiesChild->GetFBXPropertyValue<double>(4, transparencyFactor))
                            {
                                material.transparencyFactor = transparencyFactor;
                            }
                        }
                    }
                }

                mapping.data.Set(std::move(material));
            }
            else if (fbxTemplate->name == "Texture")
            {
                FBXTexture texture;
                texture.name = nodeName;

                if (const FBXObject& relativeFilenameNode = childObject->FindChild("RelativeFilename"))
                {
                    relativeFilenameNode.GetFBXPropertyValue<String>(0, texture.filename);
                }

                if (texture.filename.Empty())
                {
                    if (const FBXObject& filenameNode = childObject->FindChild("FileName"))
                    {
                        filenameNode.GetFBXPropertyValue<String>(0, texture.filename);
                    }
                }

                mapping.data.Set(std::move(texture));
            }
            else if (fbxTemplate->name == "AnimationCurve")
            {
                FBXAnimCurve curve;
                bool curveValid = true;

                if (const FBXObject& keyTimeNode = childObject->FindChild("KeyTime"))
                {
                    Result result = ReadBinaryArray<int64>(keyTimeNode, curve.keyTimes);

                    if (!result)
                    {
                        HYP_LOG(Assets, Warning, "Failed to read FBX animation curve key times: {}", result.GetError().GetMessage());

                        curveValid = false;
                    }
                }

                if (curveValid)
                {
                    if (const FBXObject& keyValueNode = childObject->FindChild("KeyValueFloat"))
                    {
                        Array<float, FBXAllocator> keyValues;
                        Result result = ReadBinaryArray<float>(keyValueNode, keyValues);

                        if (!result)
                        {
                            HYP_LOG(Assets, Warning, "Failed to read FBX animation curve key values: {}", result.GetError().GetMessage());

                            curveValid = false;
                        }
                        else
                        {
                            curve.keyValues = std::move(keyValues);
                        }
                    }
                }

                if (curveValid && curve.keyTimes.Size() == curve.keyValues.Size() && curve.keyTimes.Any())
                {
                    mapping.data.Set(std::move(curve));
                }
            }
            else if (fbxTemplate->name == "AnimationCurveNode")
            {
                mapping.data.Set(FBXAnimCurveNode {});
            }

            objectMapping[objectId] = std::move(mapping);
        }
    }

    for (const FBXConnection& connection : connections)
    {
#define INVALID_NODE_CONNECTION(msg)                                                                      \
    {                                                                                                     \
        String leftType, leftName, rightType, rightName;                                                  \
        if (leftIt != objectMapping.End())                                                                \
        {                                                                                                 \
            leftIt->second.object->GetFBXPropertyValue<String>(1, leftType);                              \
            leftName = leftIt->second.object->name.Data();                                                \
        }                                                                                                 \
        else                                                                                              \
        {                                                                                                 \
            leftType = "<not found>";                                                                     \
            leftName = "<not found>";                                                                     \
        }                                                                                                 \
        if (rightIt != objectMapping.End())                                                               \
        {                                                                                                 \
            rightIt->second.object->GetFBXPropertyValue<String>(1, rightType);                            \
            rightName = rightIt->second.object->name.Data();                                              \
        }                                                                                                 \
        else                                                                                              \
        {                                                                                                 \
            rightType = "<not found>";                                                                    \
            rightName = "<not found>";                                                                    \
        }                                                                                                 \
                                                                                                          \
        HYP_LOG(Assets, Warning, "Invalid fbx_node connection: {} \"{}\" ({}) -> {} \"{}\" ({})\n\t" msg, \
                leftType.Data(),                                                                          \
                leftName.Data(),                                                                          \
                connection.left,                                                                          \
                rightType.Data(),                                                                         \
                rightName.Data(),                                                                         \
                connection.right);                                                                        \
        continue;                                                                                         \
    }

        if (connection.left == 0)
        {
            continue;
        }

        const auto leftIt = objectMapping.Find(connection.left);
        const auto rightIt = connection.right != 0 ? objectMapping.Find(connection.right) : objectMapping.End();

        if (leftIt == objectMapping.End())
        {
            INVALID_NODE_CONNECTION("Left Id not found in fbx_node map");
        }

        if (!leftIt->second.data.IsValid())
        {
            INVALID_NODE_CONNECTION("Left fbx_node has no valid data");
        }

        if (connection.right == 0)
        {
            if (auto* leftNode = leftIt->second.data.TryGet<FBXNode>())
            {
                leftNode->parentId = 0;
                rootFbxNode.childIds.Insert(leftIt->first);

                continue;
            }
        }

        if (rightIt == objectMapping.End())
        {
            INVALID_NODE_CONNECTION("Right Id not found in fbx_node map");
        }

        if (!rightIt->second.data.IsValid())
        {
            INVALID_NODE_CONNECTION("Right fbx_node has no valid data");
        }

        if (auto* leftMesh = leftIt->second.data.TryGet<FBXMesh>())
        {
            if (auto* rightNode = rightIt->second.data.TryGet<FBXNode>())
            {
                rightNode->meshId = leftIt->first;

                continue;
            }
        }
        else if (auto* leftNode = leftIt->second.data.TryGet<FBXNode>())
        {
            if (auto* rightNode = rightIt->second.data.TryGet<FBXNode>())
            {
                if (leftNode->parentId)
                {
                    HYP_LOG(Assets, Warning, "Left fbx_node already has parent, cannot add to right fbx_node");
                }
                else
                {
                    leftNode->parentId = rightIt->first;
                    rightNode->childIds.Insert(leftIt->first);

                    continue;
                }
            }
            else if (auto* rightCluster = rightIt->second.data.TryGet<FBXCluster>())
            {
                if (leftNode->srcObject->IsFBXType("FbxLimbNode"))
                {
                    rightCluster->limbId = leftIt->first;

                    continue;
                }
            }
        }
        else if (auto* leftCluster = leftIt->second.data.TryGet<FBXCluster>())
        {
            if (auto* rightSkin = rightIt->second.data.TryGet<FBXSkin>())
            {
                HYP_LOG(Assets, Verbose, "Attach cluster to Skin");

                rightSkin->clusterIds.Insert(leftIt->first);

                continue;
            }
        }
        else if (auto* leftSkin = leftIt->second.data.TryGet<FBXSkin>())
        {
            if (auto* rightMesh = rightIt->second.data.TryGet<FBXMesh>())
            {
                if (rightMesh->skinId)
                {
                    HYP_LOG(Assets, Warning, "FBX Mesh {} already has a Skin attachment", leftIt->first);
                }

                rightMesh->skinId = leftIt->first;

                continue;
            }
        }
        else if (auto* leftMaterial = leftIt->second.data.TryGet<FBXMaterial>())
        {
            if (auto* rightNode = rightIt->second.data.TryGet<FBXNode>())
            {
                if (!rightNode->materialId)
                {
                    rightNode->materialId = leftIt->first;
                }

                continue;
            }
        }
        else if (auto* leftTexture = leftIt->second.data.TryGet<FBXTexture>())
        {
            if (auto* rightMaterial = rightIt->second.data.TryGet<FBXMaterial>())
            {
                rightMaterial->textureConnections[connection.propertyName] = leftIt->first;

                continue;
            }
        }
        else if (auto* leftCurve = leftIt->second.data.TryGet<FBXAnimCurve>())
        {
            if (auto* rightCurveNode = rightIt->second.data.TryGet<FBXAnimCurveNode>())
            {
                rightCurveNode->curveIds[connection.propertyName] = leftIt->first;

                continue;
            }
        }
        else if (auto* leftCurveNode = leftIt->second.data.TryGet<FBXAnimCurveNode>())
        {
            if (auto* rightNode = rightIt->second.data.TryGet<FBXNode>())
            {
                leftCurveNode->targetNodeId = rightIt->first;
                leftCurveNode->targetProperty = connection.propertyName;

                continue;
            }

            continue;
        }

        INVALID_NODE_CONNECTION("Unhandled connection type");

#undef INVALID_NODE_CONNECTION
    }

    Handle<Bone> rootBone = MakeHandle<Bone>();
    rootSkeleton->SetRootBone(rootBone);

    bool foundFirstBone = false;

    Map<FBXObjectID, Handle<Material>> materialCache;
    Map<FBXObjectID, Handle<Texture>> textureCache;

    {
        Map<FBXObjectID, String> texturePathsById;

        for (auto& mappingIt : objectMapping)
        {
            auto* fbxTexture = mappingIt.second.data.TryGet<FBXTexture>();

            if (!fbxTexture || fbxTexture->filename.Empty())
            {
                continue;
            }

            const String normalizedFilename = fbxTexture->filename.ReplaceAll("\\", "/");

            String resolvedPath;

            for (const String& candidate : Array<String> {
                     FilePath::Join(basePath, FilePath(normalizedFilename).Basename()),
                     FilePath::Join(basePath, normalizedFilename),
                     normalizedFilename })
            {
                if (candidate.Any() && FilePath(candidate).Exists())
                {
                    resolvedPath = candidate;

                    break;
                }
            }

            if (resolvedPath.Empty())
            {
                HYP_LOG(Assets, Warning, "FBX texture '{}' could not be resolved to an existing file relative to '{}'", fbxTexture->filename, basePath);

                continue;
            }

            texturePathsById[mappingIt.first] = resolvedPath;
        }

        if (texturePathsById.Any())
        {
            AssetBatch* texturesBatch = state.assetManager->CreateBatch(state.batchIdentifier);

            for (auto& it : texturePathsById)
            {
                texturesBatch->Add(String::ToString(it.first), it.second);
            }

            AssetMap loadedTextures = texturesBatch->ForceLoad();
            delete texturesBatch;

            for (auto& it : texturePathsById)
            {
                const String key = String::ToString(it.first);

                const auto loadedIt = loadedTextures.Find(key);

                if (loadedIt == loadedTextures.End() || !loadedIt->second.IsValid())
                {
                    HYP_LOG(Assets, Warning, "FBX texture failed to load from '{}'", it.second);

                    continue;
                }

                Handle<Texture> texture = loadedIt->second.ExtractAs<Texture>();

                if (!texture || !texture->IsCreated())
                {
                    HYP_LOG(Assets, Warning, "FBX texture loaded from '{}' has no valid GPU image, skipping", it.second);

                    continue;
                }

                FBXTexture* fbxTexture;

                if (getFbxObject(it.first, fbxTexture) && fbxTexture->name.Any())
                {
                    texture->SetName(CreateNameFromDynamicString(fbxTexture->name));
                }

                GetCurrentAssetRegistry()->PutAssetUnique(texture);

                textureCache[it.first] = std::move(texture);
            }
        }
    }

    const auto acquireTexture = [&](FBXObjectID textureId) -> Handle<Texture>
    {
        const auto it = textureCache.Find(textureId);

        return it != textureCache.End() ? it->second : Handle<Texture>::empty;
    };

    const auto acquireMaterial = [&](FBXObjectID materialId) -> Handle<Material>
    {
        if (const auto it = materialCache.Find(materialId); it != materialCache.End())
        {
            return it->second;
        }

        Handle<Material> result;

        FBXMaterial* fbxMaterial;

        if (getFbxObject(materialId, fbxMaterial))
        {
            MaterialAttributes attributes;
            attributes.shaderName = NAME("GeometryPass");
            attributes.shaderProperties = {};
            attributes.bucket = RenderBucket::Opaque;

            MaterialParameters parameters;
            parameters.albedo = Vec4f(fbxMaterial->diffuseColor.GetXYZ(), 1.0f);
            parameters.roughness = 0.65f;
            parameters.metalness = 0.0f;

            MaterialTextures textures {};

            const auto assignTexture = [&](const UTF8StringView& fbxPropertyName, MaterialTextureKey key)
            {
                const auto textureConnectionIt = fbxMaterial->textureConnections.FindAs(fbxPropertyName);

                if (textureConnectionIt == fbxMaterial->textureConnections.End())
                {
                    return;
                }

                if (Handle<Texture> texture = acquireTexture(textureConnectionIt->second); texture.IsValid())
                {
                    textures[key] = texture;
                }
            };

            assignTexture("DiffuseColor", MaterialTextureKey::Diffuse);
            assignTexture("NormalMap", MaterialTextureKey::Normals);
            assignTexture("Bump", MaterialTextureKey::Normals);

            result = MakeHandle<Material>(
                CreateNameFromDynamicString(fbxMaterial->name),
                attributes,
                parameters,
                textures);

            InitObject(result);

            GetCurrentAssetRegistry()->PutAssetUnique(result);
        }

        materialCache[materialId] = result;

        return result;
    };

    Proc<void(FBXNode&, Node*)> buildNodes;

    buildNodes = [&](FBXNode& fbxNode, Node* parentNode)
    {
        Assert(parentNode != nullptr);
        Assert(fbxNode.srcObject != nullptr);

        Handle<Node> node;

        if (fbxNode.srcObject->IsFBXType("LimbNode"))
        {
            Transform bindingTransform;
            bindingTransform.SetTranslation(fbxNode.localBindMatrix.ExtractTranslation());
            bindingTransform.SetRotation(fbxNode.localBindMatrix.ExtractRotation());
            bindingTransform.SetScale(fbxNode.localBindMatrix.ExtractScale());

            Handle<Bone> bone = MakeHandle<Bone>();
            bone->SetBindingTransform(bindingTransform);

            node = bone;
        }
        else
        {
            node = MakeHandle<Node>();
        }

        parentNode->AddChild(node);

        node->SetName(CreateNameFromDynamicString(fbxNode.name));
        node->SetLocalTransform(fbxNode.localTransform);

        if (fbxNode.meshId)
        {
            FBXMesh* fbxMesh;

            if (getFbxObject(fbxNode.meshId, fbxMesh))
            {
                const Handle<Skeleton> meshSkeleton = applyClustersToMesh(*fbxMesh);

                Handle<Mesh> mesh = fbxMesh->GetResultObject();

                GetCurrentAssetRegistry()->PutAssetUnique(mesh);

                Handle<Material> material = fbxNode.materialId ? acquireMaterial(fbxNode.materialId) : Handle<Material>::empty;

                if (!material.IsValid())
                {
                    MaterialAttributes attributes;
                    attributes.shaderName = NAME("GeometryPass");
                    attributes.shaderProperties = {};
                    attributes.bucket = RenderBucket::Opaque;

                    MaterialParameters parameters;
                    parameters.albedo = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
                    parameters.roughness = 0.65f;
                    parameters.metalness = 0.0f;

                    material = MakeHandle<Material>(
                        CreateNameFromDynamicString(fbxNode.name),
                        attributes,
                        parameters,
                        MaterialTextures {});

                    InitObject(material);

                    GetCurrentAssetRegistry()->PutAssetUnique(material);
                }

                Scene* scene = GetDetachedSceneForCurrentThread();

                const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();
                entity->SetLocalBounds(fbxMesh->bounds);

                entity->AddComponent<MeshComponent>(MeshComponent { mesh, material, meshSkeleton });

                if (meshSkeleton.IsValid())
                {
                    entity->SetIsDynamic(true);

                    AnimationComponent animationComponent {};
                    animationComponent.playbackState = {
                        .animationIndex = 0,
                        .status = AnimationPlaybackStatus::PLAYING,
                        .loopMode = AnimationLoopMode::REPEAT,
                        .speed = 1.0f,
                        .currentTime = 0.0f
                    };

                    entity->AddComponent<AnimationComponent>(animationComponent);
                }

                //// offset node to center of bounds
                // node->SetWorldTranslation(node->GetWorldTranslation() + fbxMesh->bounds.GetCenter());

                node->AddChild(entity);
            }
            else
            {
                HYP_LOG(Assets, Warning, "FBX Mesh with id {} not found in mapping", fbxNode.meshId);
            }
        }

        for (const FBXObjectID id : fbxNode.childIds)
        {
            if (id)
            {
                FBXNode* childNode;

                if (getFbxObject(id, childNode))
                {
                    buildNodes(*childNode, node);

                    continue;
                }

                HYP_LOG(Assets, Error, "Unsure how to build child object {}", id);
            }
        }
    };

    auto applyBindPoses = [&]()
    {
        for (const FBXObjectID id : bindPoseIds)
        {
            FBXBindPose* bindPose;

            if (!getFbxObject(id, bindPose))
            {
                HYP_LOG(Assets, Warning, "Not a valid bind pose fbx_node: {}", id);

                continue;
            }

            for (const FBXPoseNode& poseNode : bindPose->poseNodes)
            {
                FBXNode* linkedNode;

                if (!getFbxObject(poseNode.nodeId, linkedNode))
                {
                    HYP_LOG(Assets, Warning, "Linked fbx_node {} to pose fbx_node is not valid", poseNode.nodeId);

                    continue;
                }

                linkedNode->worldBindMatrix = poseNode.matrix;
            }
        }
    };

    Proc<void(FBXNode&)> applyLocalBindPose;

    applyLocalBindPose = [&](FBXNode& fbxNode)
    {
        fbxNode.localBindMatrix = fbxNode.worldBindMatrix;

        if (fbxNode.parentId)
        {
            FBXNode* parentNode;

            if (getFbxObject(fbxNode.parentId, parentNode))
            {
                fbxNode.localBindMatrix = parentNode->worldBindMatrix.Inverse() * fbxNode.localBindMatrix;
            }
        }

        for (const FBXObjectID id : fbxNode.childIds)
        {
            if (id)
            {
                FBXNode* childNode;

                if (getFbxObject(id, childNode))
                {
                    applyLocalBindPose(*childNode);
                }
            }
        }
    };

    auto calculateLocalBindPoses = [&]()
    {
        for (const FBXObjectID id : bindPoseIds)
        {
            FBXBindPose* bindPose;

            if (!getFbxObject(id, bindPose))
            {
                HYP_LOG(Assets, Warning, "Not a valid bind pose fbx_node: {}", id);

                continue;
            }

            for (const FBXPoseNode& poseNode : bindPose->poseNodes)
            {
                FBXNode* linkedNode;

                if (!getFbxObject(poseNode.nodeId, linkedNode))
                {
                    HYP_LOG(Assets, Warning, "Linked fbx_node {} to pose fbx_node is not valid", poseNode.nodeId);

                    continue;
                }

                applyLocalBindPose(*linkedNode);
            }
        }
    };

    applyBindPoses();
    calculateLocalBindPoses();

    Proc<void(FBXNode&)> buildLimbNodes;

    buildLimbNodes = [&](FBXNode& fbxNode)
    {
        if (fbxNode.srcObject->IsFBXType("FbxLimbNode"))
        {
            foundFirstBone = true;

            buildNodes(fbxNode, rootBone);
        }
        else
        {
            for (const FBXObjectID id : fbxNode.childIds)
            {
                if (id)
                {
                    FBXNode* childNode;

                    if (getFbxObject(id, childNode))
                    {
                        buildLimbNodes(*childNode);
                    }
                }
            }
        }
    };

#if 1

    Proc<FBXNode*(FBXNode&)> findFirstLimbNode;

    findFirstLimbNode = [&](FBXNode& fbxNode) -> FBXNode*
    {
        Assert(fbxNode.srcObject != nullptr);

        if (fbxNode.srcObject->IsFBXType("FbxLimbNode"))
        {
            return &fbxNode;
        }

        for (const FBXObjectID id : fbxNode.childIds)
        {
            if (id)
            {
                FBXNode* childNode;

                if (getFbxObject(id, childNode))
                {
                    if (FBXNode* limbNode = findFirstLimbNode(*childNode))
                    {
                        return limbNode;
                    }
                }
            }
        }

        return nullptr;
    };

#endif

    // prepare limb nodes first,
    // so the root skeleton's bones are built out before
    // we apply clusters to each entity mesh
    buildLimbNodes(rootFbxNode);

    for (const FBXObjectID id : rootFbxNode.childIds)
    {
        Node* parentNode = top;

        if (id)
        {
            FBXNode* childNode;

            if (getFbxObject(id, childNode))
            {
                buildNodes(*childNode, parentNode);
            }
        }
    }

    if (foundFirstBone)
    {
        GetCurrentAssetRegistry()->PutAssetUnique(rootSkeleton);
        InitObject(rootSkeleton);

        Map<FBXObjectID, Array<FBXAnimCurveNode*, FBXAllocator>, FBXAllocator> curveNodesByTarget;

        for (auto& mappingIt : objectMapping)
        {
            if (auto* curveNode = mappingIt.second.data.TryGet<FBXAnimCurveNode>())
            {
                if (curveNode->targetNodeId)
                {
                    curveNodesByTarget[curveNode->targetNodeId].PushBack(curveNode);
                }
            }
        }

        if (curveNodesByTarget.Any())
        {
            Handle<Animation> animation = MakeHandle<Animation>(NAME("Take"));

            constexpr UTF8StringView const ComponentNames[3] = { "d|X", "d|Y", "d|Z" };

            const auto getComponentCurves = [&](const Array<FBXAnimCurveNode*, FBXAllocator>& curveNodes, const UTF8StringView& propertyName, FBXAnimCurve** outCurves)
            {
                for (FBXAnimCurveNode* curveNode : curveNodes)
                {
                    if (curveNode->targetProperty != propertyName)
                    {
                        continue;
                    }

                    for (int component = 0; component < 3; component++)
                    {
                        const auto curveIdIt = curveNode->curveIds.FindAs(ComponentNames[component]);

                        if (curveIdIt == curveNode->curveIds.End())
                        {
                            continue;
                        }

                        FBXAnimCurve* curve;

                        if (getFbxObject(curveIdIt->second, curve))
                        {
                            outCurves[component] = curve;
                        }
                    }
                }
            };

            for (auto& targetIt : curveNodesByTarget)
            {
                FBXNode* targetNode;

                if (!getFbxObject(targetIt.first, targetNode) || !targetNode->srcObject->IsFBXType("FbxLimbNode"))
                {
                    continue;
                }

                FBXAnimCurve* translationCurves[3] = {};
                FBXAnimCurve* rotationCurves[3] = {};
                FBXAnimCurve* scalingCurves[3] = {};

                getComponentCurves(targetIt.second, "Lcl Translation", translationCurves);
                getComponentCurves(targetIt.second, "Lcl Rotation", rotationCurves);
                getComponentCurves(targetIt.second, "Lcl Scaling", scalingCurves);

                Array<int64, FBXAllocator> allKeyTimes;

                const auto collectTimes = [&](FBXAnimCurve* const (&curves)[3])
                {
                    for (int component = 0; component < 3; component++)
                    {
                        if (curves[component])
                        {
                            allKeyTimes.Concat(curves[component]->keyTimes);
                        }
                    }
                };

                collectTimes(translationCurves);
                collectTimes(rotationCurves);
                collectTimes(scalingCurves);

                if (allKeyTimes.Empty())
                {
                    continue;
                }

                std::sort(allKeyTimes.Begin(), allKeyTimes.End());

                Array<int64, FBXAllocator> uniqueKeyTimes;
                uniqueKeyTimes.Reserve(allKeyTimes.Size());

                for (const int64 timeUnits : allKeyTimes)
                {
                    if (uniqueKeyTimes.Empty() || uniqueKeyTimes.Back() != timeUnits)
                    {
                        uniqueKeyTimes.PushBack(timeUnits);
                    }
                }

                const Vec3f defaultTranslation = targetNode->localTransform.GetTranslation();
                const Vec3f defaultScale = targetNode->localTransform.GetScale();

                Array<Keyframe, FBXAllocator> keyframes;
                keyframes.Reserve(uniqueKeyTimes.Size());

                for (const int64 timeUnits : uniqueKeyTimes)
                {
                    const float timeSeconds = float(double(timeUnits) / double(FBXTimeUnitsPerSecond));

                    const Vec3f translation(
                        translationCurves[0] ? translationCurves[0]->Evaluate(timeSeconds, defaultTranslation.x) : defaultTranslation.x,
                        translationCurves[1] ? translationCurves[1]->Evaluate(timeSeconds, defaultTranslation.y) : defaultTranslation.y,
                        translationCurves[2] ? translationCurves[2]->Evaluate(timeSeconds, defaultTranslation.z) : defaultTranslation.z);

                    const Vec3f scale(
                        scalingCurves[0] ? scalingCurves[0]->Evaluate(timeSeconds, defaultScale.x) : defaultScale.x,
                        scalingCurves[1] ? scalingCurves[1]->Evaluate(timeSeconds, defaultScale.y) : defaultScale.y,
                        scalingCurves[2] ? scalingCurves[2]->Evaluate(timeSeconds, defaultScale.z) : defaultScale.z);

                    const Vec3f degrees(
                        rotationCurves[0] ? rotationCurves[0]->Evaluate(timeSeconds, 0.0f) : 0.0f,
                        rotationCurves[1] ? rotationCurves[1]->Evaluate(timeSeconds, 0.0f) : 0.0f,
                        rotationCurves[2] ? rotationCurves[2]->Evaluate(timeSeconds, 0.0f) : 0.0f);

                    const Vec3f radians(MathUtil::DegToRad(degrees.x), MathUtil::DegToRad(degrees.y), MathUtil::DegToRad(degrees.z));
                    const Quat4f rotation = Quat4f(radians).Inverse();

                    keyframes.EmplaceBack(timeSeconds, Transform(translation, scale, rotation));
                }

                Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(
                    NAME_FMT("Take_{}", targetNode->name),
                    CreateNameFromDynamicString(targetNode->name));

                track->SetKeyframes(keyframes);

                GetCurrentAssetRegistry()->PutAssetUnique(track);

                animation->AddTrack(track);
            }

            if (animation->NumTracks() > 0)
            {
                GetCurrentAssetRegistry()->PutAssetUnique(animation);

                rootSkeleton->SetAnimations({ animation });
            }
        }
    }

    if (Skeleton* skeleton = rootSkeleton.Get())
    {
        if (Bone* rootBone = skeleton->GetRootBone())
        {
            rootBone->SetToBindingPose();

            rootBone->CalculateBoneRotation();
            rootBone->CalculateBoneTranslation();

            rootBone->StoreBindingPose();
            rootBone->ClearPose();

            rootBone->UpdateBoneTransform();
        }
    }

    // align it to the engine's expectations.
    top->SetLocalRotation(Quat4f::AxisAngles(Vec3f(1.0f, 0.0f, 0.0f), MathUtil::DegToRad(90.0f)));
    top->Scale(0.01f);

    return LoadedAsset { MakeHandle<Prefab>(top->GetName(), top) };
}

void InitFBXLoaderMemory()
{
    if (t_fbxMemory == nullptr)
    {
        t_fbxMemory = new FBXLoaderMemory {
            FBXAllocator(),
            new SlabAllocator(sizeof(FBXObject), alignof(FBXObject), 1024)
        };
    }
}

void ShutdownFBXLoaderMemory()
{
    if (!t_fbxMemory)
    {
        return;
    }

    delete t_fbxMemory->objectAllocator;
    delete t_fbxMemory;
    t_fbxMemory = nullptr;
}

} // namespace Hyperion
