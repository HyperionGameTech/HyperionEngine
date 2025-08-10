/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/FBXModelLoader.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <scene/animation/Bone.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/AnimationComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <core/functional/Proc.hpp>

#include <core/compression/Archive.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/memory/Memory.hpp>

#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <algorithm>
#include <string>

#ifdef HYP_ZLIB
#include <zlib.h>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

constexpr char headerString[] = "Kaydara FBX Binary  ";
constexpr uint8 headerBytes[] = { 0x1A, 0x00 };

using FBXPropertyValue = Variant<int16, int32, int64, uint32, float, double, bool, String, ByteBuffer>;
using FBXObjectID = int64;

struct FBXProperty
{
    static const FBXProperty empty;

    FBXPropertyValue value;
    Array<FBXPropertyValue> arrayElements;

    explicit operator bool() const
    {
        return value.IsValid()
            || AnyOf(arrayElements, &FBXPropertyValue::IsValid);
    }
};

const FBXProperty FBXProperty::empty = {};

struct FBXObject
{
    static const FBXObject empty;

    String name;
    Array<FBXProperty> properties;
    Array<UniquePtr<FBXObject>> children;

    const FBXProperty& GetProperty(uint32 index) const
    {
        if (index >= properties.Size())
        {
            return FBXProperty::empty;
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
        for (auto& child : children)
        {
            if (child == nullptr)
            {
                continue;
            }

            if (child->name == childName)
            {
                return *child.Get();
            }
        }

        return empty;
    }

    const FBXObject& operator[](const String& childName) const
    {
        return FindChild(childName);
    }

    explicit operator bool() const
    {
        return !Empty();
    }

    bool Empty() const
    {
        return name.Empty()
            && properties.Empty()
            && children.Empty();
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
    Matrix4 transform;
    Matrix4 transformLink;
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
    Matrix4 matrix;
};

struct FBXBindPose
{
    String name;
    Array<FBXPoseNode> poseNodes;
};

struct FBXMesh
{
    FBXObjectID skinId = 0;

    Array<Vertex> vertices;
    Array<uint32> indices;
    VertexAttributeSet attributes;

    Optional<Handle<Mesh>> result;

    const Handle<Mesh>& GetResultObject()
    {
        if (!result.HasValue())
        {
            auto verticesAndIndices = Mesh::CalculateIndices(vertices);

            MeshData meshData;
            meshData.desc.meshAttributes.vertexAttributes = attributes;
            meshData.desc.numVertices = uint32(vertices.Size());
            meshData.desc.numIndices = uint32(indices.Size());
            meshData.vertexData = vertices;
            meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
            meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

            Handle<Mesh> mesh = CreateObject<Mesh>();
            mesh->SetMeshData(meshData);

            result.Set(std::move(mesh));
        }

        return result.Get();
    }
};

struct FBXNode
{
    enum class Type
    {
        NODE,
        LIMB_NODE
    };

    String name;
    Type type = Type::NODE;

    FBXObjectID parentId = 0;
    FBXObjectID skeletonHolderNodeId = 0;

    FBXObjectID meshId = 0;

    FlatSet<FBXObjectID> childIds;

    Transform localTransform;

    Matrix4 worldBindMatrix;
    Matrix4 localBindMatrix;

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

const FBXObject FBXObject::empty = {};

using FBXVersion = uint32;

static Result ReadFBXProperty(ByteReader& reader, FBXProperty& outProperty);
static Result ReadFBXPropertyValue(ByteReader& reader, uint8 type, FBXPropertyValue& outValue);
static uint64 ReadFBXOffset(ByteReader& reader, FBXVersion version);
static Result ReadFBXNode(ByteReader& reader, FBXVersion version, UniquePtr<FBXObject>& out);
static SizeType PrimitiveSize(uint8 primitiveType);
static bool ReadMagic(ByteReader& reader);

static bool ReadMagic(ByteReader& reader)
{
    if (reader.Max() < sizeof(headerString) + sizeof(headerBytes))
    {
        return false;
    }

    char readMagic[sizeof(headerString)] = { '\0' };

    reader.Read(readMagic, sizeof(headerString));

    if (Memory::StrCmp(readMagic, headerString, sizeof(headerString)) != 0)
    {
        return false;
    }

    uint8 bytes[sizeof(headerBytes)];
    reader.Read(bytes, sizeof(headerBytes));

    if (std::memcmp(bytes, headerBytes, sizeof(headerBytes)) != 0)
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

        ByteBuffer byteBuffer;
        reader.Read(length, byteBuffer);

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

static SizeType PrimitiveSize(uint8 primitiveType)
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

            ByteBuffer compressedBuffer;
            reader.Read(length, compressedBuffer);

            unsigned long compressedSize(length);
            unsigned long decompressedSize(PrimitiveSize(arrayHeldType) * numElements);

            Archive archive(std::move(compressedBuffer), decompressedSize);

            ByteBuffer decompressedBuffer;
            if (Result decompressResult = archive.Decompress(decompressedBuffer); decompressResult.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to decompress FBX array property: {}", decompressResult.GetError().GetMessage());
            }

            MemoryByteReader memoryReader(&decompressedBuffer);

            for (uint32 index = 0; index < numElements; ++index)
            {
                FBXPropertyValue value;

                Result result = ReadFBXPropertyValue(memoryReader, arrayHeldType, value);

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

static Result ReadFBXNode(ByteReader& reader, FBXVersion version, UniquePtr<FBXObject>& out)
{
    out = MakeUnique<FBXObject>();

    uint64 endPosition = ReadFBXOffset(reader, version);
    uint64 numProperties = ReadFBXOffset(reader, version);
    uint64 propertyListLength = ReadFBXOffset(reader, version);

    uint8 nameLength;
    reader.Read(&nameLength);

    ByteBuffer nameBuffer;
    reader.Read(nameLength, nameBuffer);

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
        UniquePtr<FBXObject> child;

        Result result = ReadFBXNode(reader, version, child);

        if (!result)
        {
            return result;
        }

        if (child != nullptr)
        {
            out->children.PushBack(std::move(child));
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

        for (SizeType index = 0; index < property.arrayElements.Size(); ++index)
        {
            const auto& item = property.arrayElements[index];

            if (const auto* valuePtr = item.TryGet<T>())
            {
                ary[index] = *valuePtr;
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

AssetLoadResult FBXModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    Handle<Node> top = CreateObject<Node>();
    Handle<Skeleton> rootSkeleton = CreateObject<Skeleton>();

    // Include our root dir as part of the path
    const String path = state.filepath;
    const auto currentDir = FileSystem::CurrentPath();
    const auto basePath = StringUtil::BasePath(path.Data());

    FileByteReader reader(FileSystem::Join(basePath, std::string(FilePath(path).Basename().Data())));

    if (reader.Eof())
    {
        return HYP_MAKE_ERROR(AssetLoadError, "File not open", AssetLoadError::ERR_EOF);
    }

    if (!ReadMagic(reader))
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Invalid magic header");
    }

    FBXVersion version;
    reader.Read<uint32>(&version);

    FBXObject root;

    do
    {
        UniquePtr<FBXObject> object;

        Result result = ReadFBXNode(reader, version, object);

        if (!result)
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read FBX node: {}", 0, result.GetError().GetMessage());
        }

        if (object == nullptr || object->Empty())
        {
            break;
        }

        root.children.PushBack(std::move(object));
    }
    while (true);

    if (const auto& definitionsNode = root["Definitions"])
    {
        int32 numDefinitions = 0;

        if (const auto& countNode = definitionsNode["Count"])
        {
            countNode.GetFBXPropertyValue<int32>(0, numDefinitions);
        }

        for (uint32 index = 0; index < uint32(numDefinitions); ++index)
        {
        }

        for (auto& child : definitionsNode.children)
        {

            if (child->name == "ObjectType")
            {
                String definitionName;
                child->GetFBXPropertyValue(0, definitionName);
            }
        }
    }

    const auto readMatrix = [](const FBXObject& object) -> Matrix4
    {
        if (!object)
        {
            return Matrix4::zeros;
        }

        Matrix4 matrix = Matrix4::zeros;

        if (FBXProperty matrixProperty = object.GetProperty(0))
        {
            if (matrixProperty.arrayElements.Size() == 16)
            {
                for (SizeType index = 0; index < matrixProperty.arrayElements.Size(); ++index)
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
            return Handle<Skeleton>::empty;
        }

        FBXNode* skeletonHolderNode;

        if (getFbxObject(limbNode.skeletonHolderNodeId, skeletonHolderNode))
        {
            if (skeletonHolderNode->skeleton.HasValue())
            {
                return skeletonHolderNode->skeleton.Get();
            }
        }

        return Handle<Skeleton>::empty;
    };

    const auto applyClustersToMesh = [&](FBXMesh& mesh) -> Handle<Skeleton>
    {
        if (!mesh.skinId)
        {
            return Handle<Skeleton>::empty;
        }

        FBXSkin* skin;

        if (!getFbxObject(mesh.skinId, skin))
        {
            return Handle<Skeleton>::empty;
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

                for (SizeType index = 0; index < cluster->vertexIndices.Size(); ++index)
                {
                    int32 positionIndex = cluster->vertexIndices[index];

                    if (positionIndex < 0)
                    {
                        positionIndex = (positionIndex * -1) - 1;
                    }

                    if (SizeType(positionIndex) >= mesh.vertices.Size())
                    {
                        HYP_LOG(Assets, Warning, "Position index {} out of range of vertex count {}",
                            positionIndex, mesh.vertices.Size());

                        break;
                    }

                    if (index >= cluster->boneWeights.Size())
                    {
                        HYP_LOG(Assets, Warning, "Index {} out of range of bone weights", index);

                        break;
                    }

                    const double weight = cluster->boneWeights[index];

                    Vertex& vertex = mesh.vertices[positionIndex];
                    vertex.AddBoneIndex(int(boneIndex));
                    vertex.AddBoneWeight(float(weight));
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
                HYP_LOG(Assets, Warning, "Invalid FBX Node connection, cannot get left Id value");
                continue;
            }

            if (!child->GetFBXPropertyValue<FBXObjectID>(2, connection.right))
            {
                HYP_LOG(Assets, Warning, "Invalid FBX Node connection, cannot get right Id value");
                continue;
            }

            connections.PushBack(connection);
        }
    }

    if (const FBXObject& objectsNode = root["Objects"])
    {
        for (auto& child : objectsNode.children)
        {
            FBXObjectID objectId;
            child->GetFBXPropertyValue<FBXObjectID>(0, objectId);

            String nodeName;
            child->GetFBXPropertyValue<String>(1, nodeName);

            FBXNodeMapping mapping { child.Get() };

            if (child->name == "Pose")
            {
                String poseType;
                child->GetFBXPropertyValue(2, poseType);

                if (poseType == "BindPose")
                {
                    FBXBindPose bindPose;

                    const SizeType substrIndex = nodeName.FindFirstIndex("Pose::");

                    if (substrIndex != String::notFound)
                    {
                        bindPose.name = nodeName.Substr(substrIndex);
                    }
                    else
                    {
                        bindPose.name = nodeName;
                    }

                    for (auto& poseChild : child->children)
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
            else if (child->name == "Deformer")
            {
                String deformerType;
                child->GetFBXPropertyValue(2, deformerType);

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

                    if (const auto& transformChild = child->FindChild("Transform"))
                    {
                        cluster.transform = readMatrix(transformChild);
                    }

                    if (const auto& transformLinkChild = child->FindChild("TransformLink"))
                    {
                        cluster.transformLink = readMatrix(transformLinkChild);
                    }

                    if (const auto& indicesChild = child->FindChild("Indexes"))
                    {
                        Result result = ReadBinaryArray<int32>(indicesChild, cluster.vertexIndices);

                        if (!result)
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read bone indices: {}", result.GetError().GetMessage());
                        }
                    }

                    if (const auto& weightsChild = child->FindChild("Weights"))
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
            else if (child->name == "Geometry")
            {
                Array<Vec3f> modelVertices;
                Array<uint32> modelIndices;
                VertexAttributeSet attributes;

                Array<String> layerNodeNames;

                if (const FBXObject& layerNode = (*child)["Layer"])
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

                if (const FBXObject& verticesNode = (*child)["Vertices"])
                {
                    const FBXProperty& verticesProperty = verticesNode.GetProperty(0);
                    const SizeType count = verticesProperty.arrayElements.Size();

                    if (count % 3 != 0)
                    {
                        return HYP_MAKE_ERROR(AssetLoadError, "Not a valid triangle mesh");
                    }

                    const SizeType numVertices = count / 3;

                    modelVertices.Resize(numVertices);

                    for (SizeType index = 0; index < numVertices; ++index)
                    {
                        Vector3 position;

                        for (uint32 subIndex = 0; subIndex < 3; ++subIndex)
                        {
                            const auto& arrayElements = verticesProperty.arrayElements[(index * 3) + SizeType(subIndex)];

                            union
                            {
                                float floatValue;
                                double doubleValue;
                            };

                            if (arrayElements.Get<float>(&floatValue))
                            {
                                position[subIndex] = floatValue;
                            }
                            else if (arrayElements.Get<double>(&doubleValue))
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

                if (const FBXObject& indicesNode = (*child)["PolygonVertexIndex"])
                {
                    const FBXProperty& indicesProperty = indicesNode.GetProperty(0);
                    const SizeType count = indicesProperty.arrayElements.Size();

                    if (count % 3 != 0)
                    {
                        return HYP_MAKE_ERROR(AssetLoadError, "Not a valid triangle mesh");
                    }

                    modelIndices.Resize(count);

                    for (SizeType index = 0; index < count; ++index)
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

                        if (SizeType(i) >= modelVertices.Size())
                        {
                            return HYP_MAKE_ERROR(AssetLoadError, "Index out of range");
                        }

                        modelIndices[index] = uint32(i);
                    }
                }

                Array<Vertex> vertices;
                vertices.Resize(modelIndices.Size());

                for (SizeType index = 0; index < modelIndices.Size(); ++index)
                {
                    vertices[index].SetPosition(modelVertices[modelIndices[index]]);
                }

                for (const String& name : layerNodeNames)
                {
                    if (name == "LayerElementUV")
                    {
                        if (const auto& uvNode = (*child)[name]["UV"])
                        {
                            attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0;

                            const SizeType count = uvNode.GetProperty(0).arrayElements.Size();
                        }
                    }
                    else if (name == "LayerElementNormal")
                    {
                        const FBXVertexMappingType mappingType = GetVertexMappingType((*child)[name]["MappingInformationType"]);

                        if (const auto& normalsNode = (*child)[name]["Normals"])
                        {
                            attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL;

                            const FBXProperty& normalsProperty = normalsNode.GetProperty(0);
                            const SizeType numNormals = normalsProperty.arrayElements.Size() / 3;

                            if (numNormals % 3 != 0)
                            {
                                return HYP_MAKE_ERROR(AssetLoadError, "Invalid normals count, must be triangulated");
                            }

                            for (SizeType triangleIndex = 0; triangleIndex < numNormals / 3; ++triangleIndex)
                            {
                                for (SizeType normalIndex = 0; normalIndex < 3; ++normalIndex)
                                {
                                    Vector3 normal;

                                    for (SizeType elementIndex = 0; elementIndex < 3; ++elementIndex)
                                    {
                                        const auto& arrayElements = normalsProperty.arrayElements[triangleIndex * 9 + normalIndex * 3 + elementIndex];

                                        union
                                        {
                                            float floatValue;
                                            double doubleValue;
                                        };

                                        if (arrayElements.Get<float>(&floatValue))
                                        {
                                            normal[elementIndex] = floatValue;
                                        }
                                        else if (arrayElements.Get<double>(&doubleValue))
                                        {
                                            normal[elementIndex] = float(doubleValue);
                                        }
                                        else
                                        {
                                            return HYP_MAKE_ERROR(AssetLoadError, "Invalid type for vertex position element -- must be float or double");
                                        }
                                    }

                                    vertices[triangleIndex * 3 + normalIndex].SetNormal(normal);
                                }
                            }
                        }
                    }
                }

                FBXMesh fbxMesh;
                fbxMesh.vertices = vertices;
                fbxMesh.indices = modelIndices;
                fbxMesh.attributes = staticMeshVertexAttributes | skeletonVertexAttributes;

                mapping.data.Set(std::move(fbxMesh));
            }
            else if (child->name == "Model")
            {
                String modelType;
                child->GetFBXPropertyValue<String>(2, modelType);

                Transform transform;

                for (const auto& modelChild : child->children)
                {
                    if (modelChild->name.StartsWith("Properties"))
                    {
                        for (const auto& propertiesChild : modelChild->children)
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
                                transform.SetRotation(Quaternion(ReadVec3f(*propertiesChild)));
                            }
                        }
                    }
                }

                static const FlatMap<String, FBXNode::Type> nodeTypes = {
                    { "Mesh", FBXNode::Type::NODE },
                    { "LimbNode", FBXNode::Type::LIMB_NODE },
                    { "Null", FBXNode::Type::NODE }
                };

                const auto nodeTypeIt = nodeTypes.Find(modelType);

                const FBXNode::Type nodeType = nodeTypeIt != nodeTypes.End()
                    ? nodeTypeIt->second
                    : FBXNode::Type::NODE;

                FBXNode fbxNode;
                fbxNode.name = nodeName;
                fbxNode.type = nodeType;
                fbxNode.localTransform = transform;

                mapping.data.Set(fbxNode);
            }

            objectMapping[objectId] = std::move(mapping);
        }
    }

    FBXNode rootFbxNode;
    rootFbxNode.name = "[FBX Model Root]";

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
                if (leftNode->type == FBXNode::Type::LIMB_NODE)
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
                HYP_LOG(Assets, Debug, "Attach cluster to Skin");

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

    Handle<Bone> rootBone = CreateObject<Bone>();
    rootSkeleton->SetRootBone(rootBone);

    bool foundFirstBone = false;

    Proc<void(FBXNode::Type, FBXNode&, Node*)> buildNodes;

    buildNodes = [&](FBXNode::Type type, FBXNode& fbxNode, Node* parentNode)
    {
#if 1 // temporarily disabled due to 'Internal Server Error' on MSW
        Assert(parentNode != nullptr);

        if (fbxNode.type != type)
        {
            return;
        }

        Handle<Node> node;

        if (fbxNode.type == FBXNode::Type::NODE)
        {
            node = CreateObject<Node>();
        }
        else if (fbxNode.type == FBXNode::Type::LIMB_NODE)
        {
            Transform bindingTransform;
            bindingTransform.SetTranslation(fbxNode.localBindMatrix.ExtractTranslation());
            bindingTransform.SetRotation(fbxNode.localBindMatrix.ExtractRotation());
            bindingTransform.SetScale(fbxNode.localBindMatrix.ExtractScale());

            Handle<Bone> bone = CreateObject<Bone>();
            bone->SetBindingTransform(bindingTransform);

            node = bone;
        }

        node->SetName(CreateNameFromDynamicString(fbxNode.name));

        if (fbxNode.meshId)
        {
            FBXMesh* fbxMesh;

            if (getFbxObject(fbxNode.meshId, fbxMesh))
            {
                applyClustersToMesh(*fbxMesh);

                Handle<Mesh> mesh = fbxMesh->GetResultObject();

                state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Meshes", mesh->GetAsset());

                MaterialAttributes materialAttributes {};
                materialAttributes.shaderDefinition = ShaderDefinition {
                    NAME("Forward"),
                    ShaderProperties(mesh->GetVertexAttributes())
                };

                Handle<Material> material = MaterialCache::GetInstance()->GetOrCreate(
                    CreateNameFromDynamicString(fbxNode.name),
                    { .shaderDefinition = ShaderDefinition {
                          NAME("Forward"),
                          ShaderProperties(mesh->GetVertexAttributes()) },
                        .bucket = RB_OPAQUE },
                    { { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f) }, { Material::MATERIAL_KEY_ROUGHNESS, 0.65f }, { Material::MATERIAL_KEY_METALNESS, 0.0f } });

                Handle<Scene> scene = g_engineDriver->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadId());

                const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();

                scene->GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { mesh, material });
                scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(entity, BoundingBoxComponent { mesh->GetAABB() });

                node->SetEntity(entity);
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
                    buildNodes(type, *childNode, node.Get());

                    continue;
                }

                HYP_LOG(Assets, Error, "Unsure how to build child object {}", id);
            }
        }

        parentNode->AddChild(node);
#endif
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
                fbxNode.localBindMatrix = parentNode->worldBindMatrix.Inverted() * fbxNode.localBindMatrix;
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
        if (fbxNode.type == FBXNode::Type::LIMB_NODE)
        {
            foundFirstBone = true;

            buildNodes(FBXNode::Type::LIMB_NODE, fbxNode, rootBone.Get());
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
        if (fbxNode.type == FBXNode::Type::LIMB_NODE)
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
        if (id)
        {
            FBXNode* childNode;

            if (getFbxObject(id, childNode))
            {
                buildNodes(FBXNode::Type::NODE, *childNode, top.Get());
            }
        }
    }

    if (foundFirstBone)
    {
        // Add Skeleton and AnimationController to Entities
        // AddSkeletonToEntities(rootSkeleton, top.Get());
    }

    top->UpdateWorldTransform();

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

    return LoadedAsset { top };
}

} // namespace hyperion
