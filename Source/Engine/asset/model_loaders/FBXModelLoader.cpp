/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/model_loaders/FBXModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/DetachedScene.hpp>
#include <scene/Node.hpp>

#include <scene/animation/Bone.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/AnimationComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/compression/Archive.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/memory/Memory.hpp>
#include <Core/memory/allocator/ArenaAllocator.hpp>
#include <Core/memory/allocator/SlabAllocator.hpp>

#include <engine/EngineDriver.hpp>

#include <algorithm>
#include <string>

#ifdef HYP_ZLIB
#include <zlib.h>
#endif

#include <FBXModelLoader.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

using FatVertex = TVertex<VT_Simple | VT_Skeletal>;

#pragma region Helpers

static Pair<Array<FatVertex>, Array<uint32>> CalculateIndices(const Array<FatVertex>& vertices)
{
    TMap<FatVertex, uint32> indexMap;

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

constexpr char HeaderString[] = "Kaydara FBX Binary  ";
constexpr uint8 HeaderBytes[] = { 0x1A, 0x00 };

using FBXPropertyValue = Variant<int16, int32, int64, uint32, float, double, bool, String, ByteBuffer>;
using FBXObjectID = int64;

struct FBXProperty
{
    static const FBXProperty s_empty;

    FBXPropertyValue value;
    Array<FBXPropertyValue> arrayElements;

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
    Array<struct FBXDefinitionProperty, DynamicAllocator> properties;
};

struct FBXObject
{
    static const FBXObject s_empty;

    String name;
    Array<FBXProperty> properties;
    Array<FBXObject*> children;
    const FBXTemplate* fbxTemplate = nullptr;

    const FBXProperty& GetProperty(uint32 index) const
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

    const FBXObject& FindChild(const String& childName) const
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

    HYP_FORCE_INLINE FBXObject& operator[](const String& childName)
    {
        return const_cast<FBXObject&>(static_cast<const FBXObject&>(*this)[childName]);
    }

    HYP_FORCE_INLINE const FBXObject& operator[](const String& childName) const
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
    FBXObjectID left;
    FBXObjectID right;
};

struct FBXCluster
{
    String name;
    Mat4f transform;
    Mat4f transformLink;
    Array<int32> vertexIndices;
    Array<double> boneWeights;

    FBXObjectID limbId = 0;
};

struct FBXSkin
{
    FlatSet<FBXObjectID> clusterIds;
};

struct FBXPoseNode
{
    FBXObjectID nodeId = 0;
    Mat4f matrix;
};

struct FBXBindPose
{
    String name;
    Array<FBXPoseNode> poseNodes;
};

struct FBXMesh
{
    FBXObject* srcObject = nullptr;

    String name;

    FBXObjectID skinId = 0;

    Array<float> vertexData;

    Array<uint32> indices;

    BoundingBox bounds;

    Optional<Handle<Mesh>> result;

    const Handle<Mesh>& GetResultObject()
    {
        if (!result.HasValue())
        {
            VertexArrayView vertexArrayView {};
            vertexArrayView.floatData = vertexData.Data();
            vertexArrayView.layoutDesc = VertexInputLayoutDesc { VT_Simple | VT_Skeletal };
            vertexArrayView.vertexCount = vertexData.ByteSize() / vertexArrayView.layoutDesc.VertexSize();

            MeshDesc meshDesc;
            meshDesc.meshAttributes.inputLayout = vertexArrayView.layoutDesc;
            meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
            meshDesc.meshAttributes.topology = TOP_TRIANGLES;
            meshDesc.numVertices = uint32(vertexArrayView.vertexCount);
            meshDesc.numIndices = uint32(indices.Size());

            /*bounds = meshData.CalculateAABB();

            const Vec3f boundsMin = bounds.GetMin();
            const Vec3f boundsCenter = bounds.GetCenter();*/

            //// offset all vertices by the AABB's center
            // for (Vertex& vertex : meshData.vertexData)
            //{
            //     vertex.SetPosition(vertex.GetPosition() - boundsCenter);
            // }

            Handle<Mesh> mesh = MakeHandle<Mesh>();
            mesh->SetName(CreateNameFromDynamicString(name));
            mesh->SetMeshData(meshDesc, vertexArrayView, indices.ToByteView());

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

    FlatSet<FBXObjectID> childIds;

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
        FBXBindPose>
        data;
};

const FBXObject FBXObject::s_empty = {};

using FBXVersion = uint32;

struct FBXLoaderMemory
{
    Arena arena;
    SlabAllocator* objectAllocator;
};

thread_local FBXLoaderMemory* s_fbxMemory = nullptr;

static void InitFBXLoaderMemory()
{
    if (s_fbxMemory == nullptr)
    {
        s_fbxMemory = new FBXLoaderMemory {
            .arena = Arena(16 * 1024 * 1024) // 16 MB
        };

        s_fbxMemory->objectAllocator = new SlabAllocator(sizeof(FBXObject), alignof(FBXObject), 1024);
    }
}

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
    s_fbxMemory->objectAllocator->Free(object);
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
    out = (FBXObject*)s_fbxMemory->objectAllocator->Allocate();
    new (out) FBXObject();

    uint64 endPosition = ReadFBXOffset(reader, version);
    uint64 numProperties = ReadFBXOffset(reader, version);
    uint64 propertyListLength = ReadFBXOffset(reader, version);

    uint8 nameLength;
    reader.Read(&nameLength);

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

template <class T>
static Result ReadBinaryArray(const FBXObject& object, Array<T>& ary)
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

template <class T>
static bool GetFBXObjectInMapping(FlatMap<FBXObjectID, FBXNodeMapping>& mapping, FBXObjectID id, T*& out)
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

// static void AddSkeletonToEntities(const Handle<Skeleton> &skeleton, Node *fbxNode)
// {
//     Assert(fbxNode != nullptr);

//     if (Handle<Entity> &entity = fbxNode->GetEntity()) {
//         entity->SetSkeleton(skeleton);

//         g_engineDriver->GetComponents() << AnimationController>(entity, MakeUnique<AnimationController());
//     }

//     for (auto &child : fbxNode->GetChildren()) {
//         if (child) {
//             AddSkeletonToEntities(skeleton, child.Get());
//         }
//     }
// }

FBXModelLoader::FBXModelLoader() = default;

AssetLoadResult FBXModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    InitFBXLoaderMemory();

    Handle<Node> top = MakeHandle<Node>();
    Handle<Skeleton> rootSkeleton = MakeHandle<Skeleton>();

    // Include our root dir as part of the path
    const String path = state.filepath;
    const FilePath currentDir = FilePath::Current();
    const FilePath basePath = FilePath(path).BasePath();

    FileByteReader fbr { FilePath::Join(basePath, FilePath(path).Basename()) };

    if (fbr.Eof())
    {
        return HYP_MAKE_ERROR(AssetLoadError, "File not open", AssetLoadError::ERR_EOF);
    }

    if (!ReadMagic(fbr))
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Invalid magic header");
    }

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

    FlatMap<FBXObjectID, FBXNodeMapping> objectMapping;
    FlatSet<FBXObjectID> bindPoseIds;
    Array<FBXConnection> connections;

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

                for (size_t index = 0; index < cluster->vertexIndices.Size(); ++index)
                {
                    int32 positionIndex = cluster->vertexIndices[index];

                    if (positionIndex < 0)
                    {
                        positionIndex = (positionIndex * -1) - 1;
                    }

                    if (size_t(positionIndex) >= mesh.vertexData.Size() / sizeof(FatVertex))
                    {
                        HYP_LOG(Assets, Warning, "Position index {} out of range of vertex count {}",
                            positionIndex, mesh.vertexData.Size() / sizeof(FatVertex));

                        break;
                    }

                    if (index >= cluster->boneWeights.Size())
                    {
                        HYP_LOG(Assets, Warning, "Index {} out of range of bone weights", index);

                        break;
                    }

                    if (index >= MAX_BONE_INDICES)
                    {
                        HYP_LOG(Assets, Warning, "Too many bone indices!");

                        continue;
                    }

                    const double weight = cluster->boneWeights[index];

                    FatVertex& vertex = reinterpret_cast<FatVertex*>(mesh.vertexData.Data())[positionIndex];
                    vertex.SetBoneIndex(index, uint8(boneIndex));
                    vertex.SetBoneWeight(index, float(weight));
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
                VertexTypeMask attributes;

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

                    if (count % 3 != 0)
                    {
                        return HYP_MAKE_ERROR(AssetLoadError, "Not a valid triangle mesh");
                    }

                    modelIndices.Resize(count);

                    for (size_t index = 0; index < count; ++index)
                    {
                        int32 i = 0;

                        if (!indicesProperty.arrayElements[index].Get<int32>(&i))
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Invalid index value");
                        }

                        if (i < 0)
                        {
                            i = (i * -1) - 1;
                        }

                        if (size_t(i) >= modelVertices.Size())
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Index out of range");
                        }

                        modelIndices[index] = uint32(i);
                    }
                }

                Array<FatVertex> verticesUnpacked;
                verticesUnpacked.Resize(modelIndices.Size());

                for (size_t index = 0; index < modelIndices.Size(); ++index)
                {
                    verticesUnpacked[index].SetPosition(modelVertices[modelIndices[index]]);
                }

                for (const String& name : layerNodeNames)
                {
                    if (name == "LayerElementUV")
                    {
                        if (const FBXObject& uvNode = (*childObject)[name]["UV"])
                        {
                            const size_t count = uvNode.GetProperty(0).arrayElements.Size();
                        }
                    }
                    else if (name == "LayerElementNormal")
                    {
                        const FBXVertexMappingType mappingType = GetVertexMappingType((*childObject)[name]["MappingInformationType"]);

                        if (const FBXObject& normalsNode = (*childObject)[name]["Normals"])
                        {
                            const FBXProperty& normalsProperty = normalsNode.GetProperty(0);
                            const uint32 numNormals = uint32(normalsProperty.arrayElements.Size()) / 3;

                            if (numNormals % 3 != 0)
                            {
                                HYP_LOG(Assets, Warning, "Not a valid triangle mesh -- invalid normal count");

                                continue; // continue on, but skip normals
                            }

                            for (uint32 triangleIndex = 0; triangleIndex < numNormals / 3; ++triangleIndex)
                            {
                                for (uint32 normalIndex = 0; normalIndex < 3; ++normalIndex)
                                {
                                    Vec3f normal;

                                    for (uint32 elementIndex = 0; elementIndex < 3; elementIndex++)
                                    {
                                        const FBXPropertyValue& elem = normalsProperty.arrayElements[triangleIndex * 9 + normalIndex * 3 + elementIndex];

                                        union
                                        {
                                            float floatValue;
                                            double doubleValue;
                                        };

                                        if (elem.Get<float>(&floatValue))
                                        {
                                            normal[elementIndex] = floatValue;
                                        }
                                        else if (elem.Get<double>(&doubleValue))
                                        {
                                            normal[elementIndex] = float(doubleValue);
                                        }
                                        else
                                        {
                                            return HYP_MAKE_ERROR(AssetLoadError, "Invalid type for vertex position element -- must be float or double");
                                        }
                                    }

                                    verticesUnpacked[triangleIndex * 3 + normalIndex].SetNormal(normal);
                                }
                            }
                        }
                    }
                }

                auto newVertsAndIndices = CalculateIndices(verticesUnpacked);

                FBXMesh fbxMesh;
                fbxMesh.srcObject = childObject;
                fbxMesh.name = nodeName;
                fbxMesh.vertexData = Array<float>(reinterpret_cast<const float*>(newVertsAndIndices.first.Data()), newVertsAndIndices.first.ByteSize() / sizeof(float));
                fbxMesh.indices = std::move(newVertsAndIndices.second);

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

                            const auto ReadVec3f = [](const FBXObject& object) -> Vector3
                            {
                                double x, y, z;

                                object.GetFBXPropertyValue<double>(4, x);
                                object.GetFBXPropertyValue<double>(5, y);
                                object.GetFBXPropertyValue<double>(6, z);

                                return { float(x), float(y), float(z) };
                            };

                            if (propertiesChildName == "Lcl Translation")
                            {
                                transform.SetTranslation(ReadVec3f(*propertiesChild));
                            }
                            else if (propertiesChildName == "Lcl Scaling")
                            {
                                transform.SetScale(ReadVec3f(*propertiesChild));
                            }
                            else if (propertiesChildName == "Lcl Rotation")
                            {
                                Vec3f degrees = ReadVec3f(*propertiesChild);
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
            leftType.Data(),                                                                              \
            leftName.Data(),                                                                              \
            connection.left,                                                                              \
            rightType.Data(),                                                                             \
            rightName.Data(),                                                                             \
            connection.right);                                                                            \
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

        INVALID_NODE_CONNECTION("Unhandled connection type");

#undef INVALID_NODE_CONNECTION
    }

    Handle<Bone> rootBone = MakeHandle<Bone>();
    rootSkeleton->SetRootBone(rootBone);

    bool foundFirstBone = false;

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
                applyClustersToMesh(*fbxMesh);

                Handle<Mesh> mesh = fbxMesh->GetResultObject();

                GetCurrentAssetRegistry()->PutAssetUnique(mesh);

                MaterialAttributes attributes;
                attributes.shaderName = NAME("GeometryPass");
                attributes.shaderProperties = {};
                attributes.bucket = RenderBucket::Opaque;

                MaterialParameters parameters;
                parameters.albedo = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
                parameters.roughness = 0.65f;
                parameters.metalness = 0.0f;

                Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(
                    CreateNameFromDynamicString(fbxNode.name),
                    attributes,
                    parameters,
                    MaterialTextures {});

                InitObject(materialDefinition);

                GetCurrentAssetRegistry()->PutAssetUnique(materialDefinition);

                Handle<MaterialInstance> materialInstance = materialDefinition->CreateInstance();
                InitObject(materialInstance);

                GetCurrentAssetRegistry()->PutAssetUnique(materialInstance);

                Scene* scene = GetDetachedSceneForCurrentThread();

                const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();
                entity->SetLocalBounds(fbxMesh->bounds);

                entity->AddComponent<MeshComponent>(MeshComponent { mesh, materialInstance });

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
        // Add Skeleton and AnimationController to Entities
        // AddSkeletonToEntities(rootSkeleton, top.Get());
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

    // temp hack
    top->SetLocalRotation(Quat4f::AxisAngles(Vec3f(1.0f, 0.0f, 0.0f), MathUtil::DegToRad(-90.0f)));
    top->Scale(0.01f);

    return LoadedAsset { top };
}

} // namespace Hyperion
