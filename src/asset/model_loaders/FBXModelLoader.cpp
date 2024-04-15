/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "FBXModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <scene/Entity.hpp>
#include <scene/animation/Bone.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/AnimationComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <rendering/Material.hpp>
#include <util/fs/FsUtil.hpp>
#include <core/lib/Memory.hpp>

#include <algorithm>
#include <stack>
#include <string>

#ifdef HYP_ZLIB
#include <zlib.h>
#endif

namespace hyperion::v2 {

constexpr char header_string[] = "Kaydara FBX Binary  ";
constexpr uint8 header_bytes[] = { 0x1A, 0x00 };

using FBXPropertyValue = Variant<int16, int32, int64, uint32, float, double, bool, String, ByteBuffer>;
using FBXObjectID = int64;

struct FBXProperty
{
    static const FBXProperty empty;

    FBXPropertyValue        value;
    Array<FBXPropertyValue> array_elements;

    explicit operator bool() const
    {
        return value.IsValid() || array_elements.Any([](const FBXPropertyValue &item) { return item.IsValid(); });
    }
};

const FBXProperty FBXProperty::empty = { };

struct FBXObject
{
    static const FBXObject empty;

    String                      name;
    Array<FBXProperty>          properties;
    Array<UniquePtr<FBXObject>> children;

    const FBXProperty &GetProperty(uint index) const
    {
        if (index >= properties.Size()) {
            return FBXProperty::empty;
        }

        return properties[index];
    }

    template <class T>
    bool GetFBXPropertyValue(uint index, T &out) const
    {
        out = { };

        if (const FBXProperty &property = GetProperty(index)) {
            if (const T *ptr = property.value.TryGet<T>()) {
                out = *ptr;

                return true;
            }
        }

        return false;
    }

    const FBXObject &FindChild(const String &child_name) const
    {
        for (auto &child : children) {
            if (child == nullptr) {
                continue;
            }

            if (child->name == child_name) {
                return *child.Get();
            }
        }

        return empty;
    }

    const FBXObject &operator[](const String &child_name) const
        { return FindChild(child_name); }

    explicit operator bool() const
        { return !Empty(); }

    bool Empty() const
    {
        return name.Empty()
            && properties.Empty()
            && children.Empty();
    }
};

struct FBXDefinitionProperty
{
    uint8   type;
    String  name;
};

struct FBXConnection
{
    FBXObjectID left;
    FBXObjectID right;
};

struct FBXCluster
{
    String          name;
    Matrix4         transform;
    Matrix4         transform_link;
    Array<int32>    vertex_indices;
    Array<double>   bone_weights;

    FBXObjectID     limb_id = 0;
};

struct FBXSkin
{
    FlatSet<FBXObjectID> cluster_ids;
};

struct FBXPoseNode
{
    FBXObjectID node_id = 0;
    Matrix4     matrix;
};

struct FBXBindPose
{
    String name;
    Array<FBXPoseNode> pose_nodes;
};

struct FBXMesh
{
    FBXObjectID         skin_id = 0;

    Array<Vertex>       vertices;
    Array<Mesh::Index>  indices;
    VertexAttributeSet  attributes;

    Optional<Handle<Mesh>> result;

    const Handle<Mesh> &GetResultObject()
    {
        if (!result.HasValue()) {
            auto vertices_and_indices = Mesh::CalculateIndices(vertices);

            auto handle = CreateObject<Mesh>(
                vertices_and_indices.first,
                vertices_and_indices.second,
                Topology::TRIANGLES,
                attributes
            );

            result.Set(std::move(handle));
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

    String                      name;
    Type                        type = Type::NODE;

    FBXObjectID                 parent_id = 0;
    FBXObjectID                 skeleton_holder_node_id = 0;

    FBXObjectID                 mesh_id = 0;

    FlatSet<FBXObjectID>        child_ids;

    Transform                   local_transform;

    Matrix4                     world_bind_matrix;
    Matrix4                     local_bind_matrix;

    Optional<Handle<Skeleton>>  skeleton;
    //Optional<NodeProxy> node_proxy;
};

struct FBXNodeMapping
{
    FBXObject *object;

    Variant<
        FBXMesh,
        FBXNode,
        FBXCluster,
        FBXSkin,
        FBXBindPose
    > data;
};

const FBXObject FBXObject::empty = { };

using FBXVersion = uint32;

static LoaderResult ReadFBXProperty(ByteReader &reader, FBXProperty &out_property);
static LoaderResult ReadFBXPropertyValue(ByteReader &reader, uint8 type, FBXPropertyValue &out_value);
static uint64 ReadFBXOffset(ByteReader &reader, FBXVersion version);
static LoaderResult ReadFBXNode(ByteReader &reader, FBXVersion version, UniquePtr<FBXObject> &out);
static SizeType PrimitiveSize(uint8 primitive_type);
static bool ReadMagic(ByteReader &reader);

static bool ReadMagic(ByteReader &reader)
{
    if (reader.Max() < sizeof(header_string) + sizeof(header_bytes)) {
        return false;
    }

    char read_magic[sizeof(header_string)] = { '\0' };

    reader.Read(read_magic, sizeof(header_string));

    if (Memory::StrCmp(read_magic, header_string, sizeof(header_string)) != 0) {
        return false;
    }

    uint8 bytes[sizeof(header_bytes)];
    reader.Read(bytes, sizeof(header_bytes));

    if (std::memcmp(bytes, header_bytes, sizeof(header_bytes)) != 0) {
        return false;
    }

    return true;
}

static LoaderResult ReadFBXPropertyValue(ByteReader &reader, uint8 type, FBXPropertyValue &out_value)
{
    switch (type) {
    case 'R':
    case 'S':
        {
            uint32 length;
            reader.Read(&length);

            ByteBuffer byte_buffer;
            reader.Read(length, byte_buffer);

            if (type == 'R') {
                out_value.Set(std::move(byte_buffer));
            } else {
                out_value.Set(String(byte_buffer));
            }

            return { };
        }
    case 'Y':
        {
            // int16 (signed)
            int16 i;
            reader.Read(&i);

            out_value.Set(i);

            return { };
        }
    case 'I':
        {
            // int32 (signed)
            int32 i;
            reader.Read(&i);

            out_value.Set(i);

            return { };
        }
    case 'L':
        {
            // int64 (Signed)
            int64 i;
            reader.Read(&i);

            out_value.Set(i);

            return { };
        }
    case 'C': // fallthrough
    case 'B':
        {
            uint8 b;
            reader.Read(&b);

            out_value.Set(bool(b != 0));

            return { };
        }
    case 'F':
        {
            // float
            float f;
            reader.Read(&f);

            out_value.Set(f);

            return { };
        }
    case 'D':
        {
            // double
            double d;
            reader.Read(&d);

            out_value.Set(d);

            return { };
        }
    default:
        return { LoaderResult::Status::ERR, String("Invalid property type '") + String::ToString(int(type)) + "'" };
    }
}

static SizeType PrimitiveSize(uint8 primitive_type)
{
    switch (primitive_type) {
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

static LoaderResult ReadFBXProperty(ByteReader &reader, FBXProperty &out_property)
{
    uint8 type;
    reader.Read(&type);

    if (type < 'Z') {
        LoaderResult result = ReadFBXPropertyValue(reader, type, out_property.value);

        if (!result) {
            return result;
        }
    } else {
        // array -- can be zlib compressed

        const uint8 array_held_type = type - ('a' - 'A');

        uint32 num_elements;
        reader.Read(&num_elements);

        uint32 encoding;
        reader.Read(&encoding);

        uint32 length;
        reader.Read(&length);

        if (encoding != 0) {
            ByteBuffer compressed_buffer;
            reader.Read(length, compressed_buffer);

#ifdef HYP_ZLIB
            unsigned long compressed_size(length);
            unsigned long decompressed_size(PrimitiveSize(array_held_type) * num_elements);

            ByteBuffer decompressed_buffer(decompressed_size);

            const unsigned long compressed_size_before = compressed_size;
            const unsigned long decompressed_size_before = decompressed_size;

            if (uncompress2(decompressed_buffer.Data(), &decompressed_size, compressed_buffer.Data(), &compressed_size) != Z_OK) {
                return { LoaderResult::Status::ERR, "Failed to decompress data" };
            }

            if (compressed_size != compressed_size_before || decompressed_size != decompressed_size_before) {
                return { LoaderResult::Status::ERR, "Decompressed data was incorrect" };
            }

            MemoryByteReader memory_reader(&decompressed_buffer);

            for (uint32 index = 0; index < num_elements; ++index) {
                FBXPropertyValue value;

                LoaderResult result = ReadFBXPropertyValue(memory_reader, array_held_type, value);

                if (!result) {
                    return result;
                }

                out_property.array_elements.PushBack(std::move(value));
            }
#endif
        } else {
            for (uint32 index = 0; index < num_elements; ++index) {
                FBXPropertyValue value;

                LoaderResult result = ReadFBXPropertyValue(reader, array_held_type, value);

                if (!result) {
                    return result;
                }

                out_property.array_elements.PushBack(std::move(value));
            }
        }
    }

    return { };
}

static uint64 ReadFBXOffset(ByteReader &reader, FBXVersion version)
{
    if (version >= 7500) {
        uint64 value;
        reader.Read(&value);

        return value;
    } else {
        uint32 value;
        reader.Read(&value);

        return uint64(value);
    }
}

static LoaderResult ReadFBXNode(ByteReader &reader, FBXVersion version, UniquePtr<FBXObject> &out)
{
    out.Reset(new FBXObject);

    uint64 end_position = ReadFBXOffset(reader, version);
    uint64 num_properties = ReadFBXOffset(reader, version);
    uint64 property_list_length = ReadFBXOffset(reader, version);

    uint8 name_length;
    reader.Read(&name_length);

    ByteBuffer name_buffer;
    reader.Read(name_length, name_buffer);

    out->name = String(name_buffer);

    for (uint32 index = 0; index < num_properties; ++index) {
        FBXProperty property;

        LoaderResult result = ReadFBXProperty(reader, property);

        if (!result) {
            return result;
        }

        if (property) {
            out->properties.PushBack(std::move(property));
        }
    }

    while (reader.Position() < end_position) {
        UniquePtr<FBXObject> child;

        LoaderResult result = ReadFBXNode(reader, version, child);

        if (!result) {
            return result;
        }

        if (child != nullptr) {
            out->children.PushBack(std::move(child));
        }
    }

    return { };
}


enum FBXVertexMappingType
{
    INVALID_MAPPING_TYPE = 0,
    BY_POLYGON_VERTEX,
    BY_POLYGON,
    BY_VERTEX
};

static FBXVertexMappingType GetVertexMappingType(const FBXObject &object)
{
    FBXVertexMappingType mapping_type = INVALID_MAPPING_TYPE;

    if (object) {
        String mapping_type_str;

        if (object.GetFBXPropertyValue<String>(0, mapping_type_str)) {
            if (mapping_type_str == "ByPolygonVertex") {
                mapping_type = BY_POLYGON_VERTEX;
            } else if (mapping_type_str == "ByPolygon") {
                mapping_type = BY_POLYGON;
            } else if (mapping_type_str.StartsWith("ByVert")) {
                mapping_type = BY_VERTEX;
            }
        }
    }

    return mapping_type;
}

template <class T>
static LoaderResult ReadBinaryArray(const FBXObject &object, Array<T> &ary)
{
    if (FBXProperty property = object.GetProperty(0)) {
        ary.Resize(property.array_elements.Size());

        for (SizeType index = 0; index < property.array_elements.Size(); ++index) {
            const auto &item = property.array_elements[index];

            if (const auto *value_ptr = item.TryGet<T>()) {
                ary[index] = *value_ptr;
            } else {
                return { LoaderResult::Status::ERR, "Type mismatch for array data" };
            }
        }
    }

    return { LoaderResult::Status::OK };
}

template <class T>
static bool GetFBXObjectInMapping(FlatMap<FBXObjectID, FBXNodeMapping> &mapping, FBXObjectID id, T *&out)
{
    out = nullptr;

    const auto it = mapping.Find(id);

    if (it == mapping.End()) {
        return false;
    }

    if (auto *ptr = it->second.data.TryGet<T>()) {
        out = ptr;

        return true;
    }

    return false;
}

// static void AddSkeletonToEntities(const Handle<Skeleton> &skeleton, Node *node)
// {
//     AssertThrow(node != nullptr);

//     if (Handle<Entity> &entity = node->GetEntity()) {
//         entity->SetSkeleton(skeleton);

//         g_engine->GetComponents().Add<AnimationController>(entity, UniquePtr<AnimationController>::Construct());
//     }

//     for (auto &child : node->GetChildren()) {
//         if (child) {
//             AddSkeletonToEntities(skeleton, child.Get());
//         }
//     }
// }

LoadedAsset FBXModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    auto top = UniquePtr<Node>::Construct();
    Handle<Skeleton> root_skeleton = CreateObject<Skeleton>();

    // Include our root dir as part of the path
    const String path = state.filepath;
    const auto current_dir = FileSystem::CurrentPath();
    const auto base_path = StringUtil::BasePath(path.Data());

    FileByteReader reader(FileSystem::Join(base_path, std::string(FilePath(path).Basename().Data())));

    if (reader.Eof()) {
        return { { LoaderResult::Status::ERR, "File not open" } };
    }

    if (!ReadMagic(reader)) {
        return { { LoaderResult::Status::ERR, "Invalid magic header" } };
    }

    FBXVersion version;
    reader.Read<uint32>(&version);

    FBXObject root;

    do {
        UniquePtr<FBXObject> object;

        LoaderResult result = ReadFBXNode(reader, version, object);

        if (!result) {
            return { result };
        }

        if (object == nullptr || object->Empty()) {
            break;
        }

        root.children.PushBack(std::move(object));
    } while (true);

    FlatMap<String, FBXDefinitionProperty> definitions;

    if (const auto &definitions_node = root["Definitions"]) {
        int32 num_definitions = 0;

        if (const auto &count_node = definitions_node["Count"]) {
            count_node.GetFBXPropertyValue<int32>(0, num_definitions);
        }

        for (uint index = 0; index < uint(num_definitions); ++index) {

        }

        for (auto &child : definitions_node.children) {

            if (child->name == "ObjectType") {
                String definition_name;
                child->GetFBXPropertyValue(0, definition_name);
            }
        }
    }

    const auto ReadMatrix = [](const FBXObject &object) -> Matrix4 {
        if (!object) {
            return Matrix4::zeros;
        }

        Matrix4 matrix = Matrix4::zeros;

        if (FBXProperty matrix_property = object.GetProperty(0)) {
            if (matrix_property.array_elements.Size() == 16) {
                for (SizeType index = 0; index < matrix_property.array_elements.Size(); ++index) {
                    const auto &item = matrix_property.array_elements[index];

                    if (const auto *value_double = item.TryGet<double>()) {
                        matrix.values[index] = float(*value_double);
                    } else if (const auto *value_float = item.TryGet<float>()) {
                        matrix.values[index] = *value_float;
                    }
                }

            } else {
                DebugLog(
                    LogType::Warn,
                    "Invalid matrix in FBX node -- invalid size\n"
                );
            }
        }

        return matrix;
    };

    FlatMap<FBXObjectID, FBXNodeMapping> object_mapping;
    FlatSet<FBXObjectID> bind_pose_ids;
    Array<FBXConnection> connections;

    const auto GetFBXObject = [&object_mapping](FBXObjectID id, auto *&out) {
        return GetFBXObjectInMapping(object_mapping, id, out);
    };

    const auto GetSkeletonFromLimbNode = [&](const FBXNode &limb_node) -> Handle<Skeleton> {
        if (!limb_node.skeleton_holder_node_id) {
            return Handle<Skeleton>::empty;
        }

        FBXNode *skeleton_holder_node;

        if (GetFBXObject(limb_node.skeleton_holder_node_id, skeleton_holder_node)) {
            if (skeleton_holder_node->skeleton.HasValue()) {
                return skeleton_holder_node->skeleton.Get();
            }
        }

        return Handle<Skeleton>::empty;
    };

    const auto ApplyClustersToMesh = [&](FBXMesh &mesh) -> Handle<Skeleton> {
        if (!mesh.skin_id) {
            return Handle<Skeleton>::empty;
        }

        FBXSkin *skin;

        if (!GetFBXObject(mesh.skin_id, skin)) {
            return Handle<Skeleton>::empty;
        }

        Handle<Skeleton> skeleton = root_skeleton;

        for (const FBXObjectID cluster_id : skin->cluster_ids) {
            FBXCluster *cluster;

            if (!GetFBXObject(cluster_id, cluster)) {
                DebugLog(
                    LogType::Warn,
                    "Cluster with id %lld not found in mapping!\n",
                    cluster_id
                );

                continue;
            }

            if (cluster->limb_id) {
                FBXNode *limb_node;

                if (!GetFBXObject(cluster->limb_id, limb_node)) {
                    DebugLog(
                        LogType::Warn,
                        "LimbNode with id %lld not found in mapping!\n",
                        cluster->limb_id
                    );

                    continue;
                }

#if 0
                Handle<Skeleton> skeleton_from_limb = GetSkeletonFromLimbNode(*limb_node);

                if (skeleton && skeleton_from_limb && skeleton != skeleton_from_limb) {
                    DebugLog(
                        LogType::Warn,
                        "LimbNode with id %lld has Skeleton with ID #%u, but multiple skeletons are attached to the mesh!\n",
                        cluster->limb_id,
                        skeleton_from_limb->GetID()
                    );
                }

                skeleton = skeleton_from_limb;

                if (!skeleton) {
                    DebugLog(
                        LogType::Warn,
                        "LimbNode with id %lld has no Skeleton!\n",
                        cluster->limb_id
                    );

                    continue;
                }
#endif

                const uint bone_index = skeleton->FindBoneIndex(limb_node->name);

                if (bone_index == uint(-1)) {
                    DebugLog(
                        LogType::Warn,
                        "LimbNode with id %lld: Bone with name %s not found in Skeleton\n",
                        cluster->limb_id,
                        limb_node->name.Data()
                    );

                    continue;
                }

                for (SizeType index = 0; index < cluster->vertex_indices.Size(); ++index) {
                    int32 position_index = cluster->vertex_indices[index];

                    if (position_index < 0) {
                        position_index = (position_index * -1) - 1;
                    }

                    if (SizeType(position_index) >= mesh.vertices.Size()) {
                        DebugLog(
                            LogType::Warn,
                            "Position index (%d) out of range of vertex count (%llu)\n",
                            position_index,
                            mesh.vertices.Size()
                        );

                        break;
                    }

                    if (index >= cluster->bone_weights.Size()) {
                        DebugLog(
                            LogType::Warn,
                            "Index (%llu) out of range of bone weights\n",
                            index
                        );

                        break;
                    }

                    const double weight = cluster->bone_weights[index];

                    Vertex &vertex = mesh.vertices[position_index];
                    vertex.AddBoneIndex(int(bone_index));
                    vertex.AddBoneWeight(float(weight));
                }
            }
        }

        return skeleton;
    };

    if (const FBXObject &connections_node = root["Connections"]) {
        for (auto &child : connections_node.children) {
            FBXConnection connection { };

            if (!child->GetFBXPropertyValue<FBXObjectID>(1, connection.left)) {
                DebugLog(LogType::Warn, "Invalid FBX Node connection, cannot get left ID value\n");
                continue;
            }

            if (!child->GetFBXPropertyValue<FBXObjectID>(2, connection.right)) {
                DebugLog(LogType::Warn, "Invalid FBX Node connection, cannot get right ID value\n");
                continue;
            }

            connections.PushBack(connection);
        }
    }

    if (const FBXObject &objects_node = root["Objects"]) {
        for (auto &child : objects_node.children) {
            FBXObjectID object_id;
            child->GetFBXPropertyValue<FBXObjectID>(0, object_id);

            String node_name;
            child->GetFBXPropertyValue<String>(1, node_name);

            FBXNodeMapping mapping { child.Get() };

            if (child->name == "Pose") {
                String pose_type;
                child->GetFBXPropertyValue(2, pose_type);

                if (pose_type == "BindPose") {
                    FBXBindPose bind_pose;

                    const SizeType substr_index = node_name.FindIndex("Pose::");

                    if (substr_index != String::not_found) {
                        bind_pose.name = node_name.Substr(substr_index);
                    } else {
                        bind_pose.name = node_name;
                    }

                    for (auto &pose_child : child->children) {
                        if (pose_child->name == "PoseNode") {
                            FBXPoseNode pose;

                            if (const FBXObject &node_node = pose_child->FindChild("Node")) {
                                node_node.GetFBXPropertyValue<FBXObjectID>(0, pose.node_id);
                            }

                            pose.matrix = ReadMatrix(pose_child->FindChild("Matrix"));

                            bind_pose.pose_nodes.PushBack(pose);
                        }
                    }

                    mapping.data.Set(std::move(bind_pose));

                    bind_pose_ids.Insert(object_id);
                } else {
                    DebugLog(
                        LogType::Debug,
                        "Unsure how to handle Pose type %s\n",
                        pose_type.Data()
                    );

                    continue;
                }
            } else if (child->name == "Deformer") {
                String deformer_type;
                child->GetFBXPropertyValue(2, deformer_type);

                if (deformer_type == "Skin") {
                    mapping.data.Set(FBXSkin { });
                } else if (deformer_type == "Cluster") {
                    FBXCluster cluster;

                    const auto name_split = node_name.Split(':');

                    if (name_split.Size() > 1) {
                        cluster.name = name_split[1];
                    }

                    if (const auto &transform_child = child->FindChild("Transform")) {
                        cluster.transform = ReadMatrix(transform_child);
                    }

                    if (const auto &transform_link_child = child->FindChild("TransformLink")) {
                        cluster.transform_link = ReadMatrix(transform_link_child);
                    }

                    if (const auto &indices_child = child->FindChild("Indexes")) {
                        auto result = ReadBinaryArray<int32>(indices_child, cluster.vertex_indices);

                        if (!result) {
                            return result;
                        }
                    }

                    if (const auto &weights_child = child->FindChild("Weights")) {
                        auto result = ReadBinaryArray<double>(weights_child, cluster.bone_weights);

                        if (!result) {
                            return result;
                        }
                    }

                    mapping.data.Set(cluster);
                } else {
                    DebugLog(
                        LogType::Debug,
                        "Unsure how to handle Deformer type %s\n",
                        deformer_type.Data()
                    );

                    continue;
                }
            } else if (child->name == "Geometry") {
                Array<Vector3> model_vertices;
                Array<Mesh::Index> model_indices;
                VertexAttributeSet attributes;

                Array<String> layer_node_names;

                if (const FBXObject &layer_node = (*child)["Layer"]) {
                    for (const auto &layer_node_child : layer_node.children) {
                        if (layer_node_child->name == "LayerElement") {
                            if (const auto &layer_node_type = (*layer_node_child)["Type"]) {
                                String layer_node_type_name;

                                if (layer_node_type.GetFBXPropertyValue(0, layer_node_type_name)) {
                                    layer_node_names.PushBack(std::move(layer_node_type_name));
                                }
                            }
                        }
                    }
                }

                if (const FBXObject &vertices_node = (*child)["Vertices"]) {
                    const FBXProperty &vertices_property = vertices_node.GetProperty(0);
                    const SizeType count = vertices_property.array_elements.Size();

                    if (count % 3 != 0) {
                        return { { LoaderResult::Status::ERR, "Not a valid vertices array" } };
                    }

                    const SizeType num_vertices = count / 3;

                    model_vertices.Resize(num_vertices);

                    for (SizeType index = 0; index < num_vertices; ++index) {
                        Vector3 position;

                        for (uint sub_index = 0; sub_index < 3; ++sub_index) {
                            const auto &array_elements = vertices_property.array_elements[(index * 3) + SizeType(sub_index)];

                            union { float float_value; double double_value; };

                            if (array_elements.Get<float>(&float_value)) {
                                position[sub_index] = float_value;
                            } else if (array_elements.Get<double>(&double_value)) {
                                position[sub_index] = float(double_value);
                            } else {
                                return { { LoaderResult::Status::ERR, "Invalid type for vertex position element -- must be float or double" } };
                            }
                        }

                        model_vertices[index] = position;
                    }
                }

                if (const FBXObject &indices_node = (*child)["PolygonVertexIndex"]) {
                    const FBXProperty &indices_property = indices_node.GetProperty(0);
                    const SizeType count = indices_property.array_elements.Size();

                    if (count % 3 != 0) {
                        return { { LoaderResult::Status::ERR, "Not a valid triangle mesh" } };
                    }

                    model_indices.Resize(count);

                    for (SizeType index = 0; index < count; ++index) {
                        int32 i = 0;

                        if (!indices_property.array_elements[index].Get<int32>(&i)) {
                            return { { LoaderResult::Status::ERR, "Invalid index value" } };
                        }

                        if (i < 0) {
                            i = (i * -1) - 1;
                        }

                        if (SizeType(i) >= model_vertices.Size()) {
                            return { { LoaderResult::Status::ERR, "Index out of range" } };
                        }

                        model_indices[index] = Mesh::Index(i);
                    }
                }

                Array<Vertex> vertices;
                vertices.Resize(model_indices.Size());

                for (SizeType index = 0; index < model_indices.Size(); ++index) {
                    vertices[index].SetPosition(model_vertices[model_indices[index]]);
                }

                for (const String &name : layer_node_names) {
                    if (name == "LayerElementUV") {
                        if (const auto &uv_node = (*child)[name]["UV"]) {
                            attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0;

                            const SizeType count = uv_node.GetProperty(0).array_elements.Size();
                        }
                    } else if (name == "LayerElementNormal") {
                        const FBXVertexMappingType mapping_type = GetVertexMappingType((*child)[name]["MappingInformationType"]);

                        if (const auto &normals_node = (*child)[name]["Normals"]) {
                            attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL;

                            const FBXProperty &normals_property = normals_node.GetProperty(0);
                            const SizeType num_normals = normals_property.array_elements.Size() / 3;

                            if (num_normals % 3 != 0) {
                                return { { LoaderResult::Status::ERR, "Invalid normals count, must be triangulated" } };
                            }

                            for (SizeType triangle_index = 0; triangle_index < num_normals / 3; ++triangle_index) {
                                for (SizeType normal_index = 0; normal_index < 3; ++normal_index) {
                                    Vector3 normal;

                                    for (SizeType element_index = 0; element_index < 3; ++element_index) {
                                        const auto &array_elements = normals_property.array_elements[triangle_index * 9 + normal_index * 3 + element_index];

                                        union { float float_value; double double_value; };

                                        if (array_elements.Get<float>(&float_value)) {
                                            normal[element_index] = float_value;
                                        } else if (array_elements.Get<double>(&double_value)) {
                                            normal[element_index] = float(double_value);
                                        } else {
                                            return { { LoaderResult::Status::ERR, "Invalid type for vertex position element -- must be float or double" } };
                                        }
                                    }

                                    vertices[triangle_index * 3 + normal_index].SetNormal(normal);
                                }
                            }
                        }
                    }
                }

                FBXMesh mesh;
                mesh.vertices = vertices;
                mesh.indices = model_indices;
                mesh.attributes = static_mesh_vertex_attributes | skeleton_vertex_attributes;

                mapping.data.Set(mesh);

            } else if (child->name == "Model") {
                String model_type;
                child->GetFBXPropertyValue<String>(2, model_type);

                Transform transform;

                for (const auto &model_child : child->children) {
                    if (model_child->name.StartsWith("Properties")) {
                        for (const auto &properties_child : model_child->children) {
                            String properties_child_name;

                            if (!properties_child->GetFBXPropertyValue<String>(0, properties_child_name)) {
                                continue;
                            }

                            const auto ReadVec3f = [](const FBXObject &object) -> Vector3 {
                                double x, y, z;

                                object.GetFBXPropertyValue<double>(4, x);
                                object.GetFBXPropertyValue<double>(5, y);
                                object.GetFBXPropertyValue<double>(6, z);

                                return { float(x), float(y), float(z) };
                            };

                            if (properties_child_name == "Lcl Translation") {
                                transform.SetTranslation(ReadVec3f(*properties_child));
                            } else if (properties_child_name == "Lcl Scaling") {
                                transform.SetScale(ReadVec3f(*properties_child));
                            } else if (properties_child_name == "Lcl Rotation") {
                                transform.SetRotation(Quaternion(ReadVec3f(*properties_child)));
                            }
                        }
                    }
                }

                static const FlatMap<String, FBXNode::Type> node_types = {
                    { "Mesh", FBXNode::Type::NODE },
                    { "LimbNode", FBXNode::Type::LIMB_NODE },
                    { "Null", FBXNode::Type::NODE }
                };

                const auto node_type_it = node_types.Find(model_type);

                const FBXNode::Type node_type = node_type_it != node_types.End()
                    ? node_type_it->second
                    : FBXNode::Type::NODE;

                FBXNode node;
                node.name = node_name;
                node.type = node_type;
                node.local_transform = transform;

                mapping.data.Set(node);
            }

            object_mapping[object_id] = std::move(mapping);
        }
    }

    FBXNode root_fbx_node;
    root_fbx_node.name = "[FBX Model Root]";

    for (const FBXConnection &connection : connections) {
#define INVALID_NODE_CONNECTION(msg) \
    { \
        String left_type, left_name, right_type, right_name; \
        if (left_it != object_mapping.End()) { \
            left_it->second.object->GetFBXPropertyValue<String>(1, left_type); \
            left_name = left_it->second.object->name.Data(); \
        } else { \
            left_type = "<not found>"; \
            left_name = "<not found>"; \
        } \
        if (right_it != object_mapping.End()) { \
            right_it->second.object->GetFBXPropertyValue<String>(1, right_type); \
            right_name = right_it->second.object->name.Data(); \
        } else { \
            right_type = "<not found>"; \
            right_name = "<not found>"; \
        } \
        \
        DebugLog( \
            LogType::Warn, \
            "Invalid node connection: %s \"%s\" (%u) -> %s \"%s\" (%u)\n\t" msg "\n", \
            left_type.Data(), \
            left_name.Data(), \
            connection.left, \
            right_type.Data(), \
            right_name.Data(), \
            connection.right \
        ); \
        continue; \
    }

        if (connection.left == 0) {
            continue;
        }

        const auto left_it = object_mapping.Find(connection.left);
        const auto right_it = connection.right != 0 ? object_mapping.Find(connection.right) : object_mapping.End();

        if (left_it == object_mapping.End()) {
            INVALID_NODE_CONNECTION("Left ID not found in node map");
        }

        if (!left_it->second.data.IsValid()) {
            INVALID_NODE_CONNECTION("Left node has no valid data");
        }

        if (connection.right == 0) {
            if (auto *left_node = left_it->second.data.TryGet<FBXNode>()) {
                left_node->parent_id = 0;
                root_fbx_node.child_ids.Insert(left_it->first);

                continue;
            }
        }

        if (right_it == object_mapping.End()) {
            INVALID_NODE_CONNECTION("Right ID not found in node map");
        }

        if (!right_it->second.data.IsValid()) {
            INVALID_NODE_CONNECTION("Right node has no valid data");
        }

        if (auto *left_mesh = left_it->second.data.TryGet<FBXMesh>()) {
            if (auto *right_node = right_it->second.data.TryGet<FBXNode>()) {
                right_node->mesh_id = left_it->first;

                continue;
            }

        } else if (auto *left_node = left_it->second.data.TryGet<FBXNode>()) {
            if (auto *right_node = right_it->second.data.TryGet<FBXNode>()) {
                if (left_node->parent_id) {
                    DebugLog(LogType::Warn, "Left node already has parent, cannot add to right node\n");
                } else {
                    left_node->parent_id = right_it->first;
                    right_node->child_ids.Insert(left_it->first);

                    continue;
                }
            } else if (auto *right_cluster = right_it->second.data.TryGet<FBXCluster>()) {
                if (left_node->type == FBXNode::Type::LIMB_NODE) {
                    right_cluster->limb_id = left_it->first;

                    continue;
                }
            }
        } else if (auto *left_cluster = left_it->second.data.TryGet<FBXCluster>()) {
            if (auto *right_skin = right_it->second.data.TryGet<FBXSkin>()) {
                DebugLog(LogType::Debug, "Attach cluster to Skin\n");

                right_skin->cluster_ids.Insert(left_it->first);

                continue;
            }
        } else if (auto *left_skin = left_it->second.data.TryGet<FBXSkin>()) {
            if (auto *right_mesh = right_it->second.data.TryGet<FBXMesh>()) {
                if (right_mesh->skin_id) {
                    DebugLog(
                        LogType::Warn,
                        "FBX Mesh %lld already has a Skin attachment\n",
                        left_it->first
                    );
                }

                right_mesh->skin_id = left_it->first;

                continue;
            }
        }

        INVALID_NODE_CONNECTION("Unhandled connection type");

#undef INVALID_NODE_CONNECTION
    }

    NodeProxy root_bone(new Bone);
    root_skeleton->SetRootBone(root_bone);

    bool found_first_bone = false;

    std::function<void(FBXNode::Type, FBXNode &, Node *)> BuildNodes;

    BuildNodes = [&](FBXNode::Type type, FBXNode &node, Node *parent_node)
    {
#if 0 // temporarily disabled due to 'Internal Server Error' on MSW
        AssertThrow(parent_node != nullptr);

        if (node.type != type) {
            return;
        }

        NodeProxy node_proxy;

        if (node.type == FBXNode::Type::NODE) {
            node_proxy = NodeProxy(new Node);
        } else if (node.type == FBXNode::Type::LIMB_NODE) {
            Transform binding_transform;
            binding_transform.SetTranslation(Vector3(
                node.local_bind_matrix.GetColumn(3)[0],
                node.local_bind_matrix.GetColumn(3)[1],
                node.local_bind_matrix.GetColumn(3)[2]
            ));
            binding_transform.SetRotation(Quaternion(node.local_bind_matrix));
            binding_transform.SetScale(node.local_bind_matrix.ExtractTransformScale());

            Bone *bone = new Bone;
            bone->SetBindingTransform(binding_transform);

            node_proxy = NodeProxy(bone);
        }

        node_proxy.SetName(node.name);

        if (node.mesh_id) {
            FBXMesh *mesh;

            if (GetFBXObject(node.mesh_id, mesh)) {
                ApplyClustersToMesh(*mesh);

                auto material = g_material_system->GetOrCreate({
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(mesh->attributes, {{ "SKINNING" }})
                    },
                    .bucket = Bucket::BUCKET_OPAQUE
                });

                const Handle<Scene> &detached_scene = g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID());

                const ID<Entity> entity = detached_scene->GetEntityManager()->AddEntity();

                detached_scene->GetEntityManager()->AddComponent(
                    entity,
                    TransformComponent { }
                );

                detached_scene->GetEntityManager()->AddComponent(
                    entity,
                    MeshComponent {
                        mesh->GetResultObject(),
                        material
                    }
                );

                detached_scene->GetEntityManager()->AddComponent(
                    entity,
                    BoundingBoxComponent {
                        mesh->GetResultObject()->GetAABB()
                    }
                );

                detached_scene->GetEntityManager()->AddComponent(
                    entity,
                    VisibilityStateComponent { }
                );

                node_proxy.SetEntity(entity);
            }
        }

        for (const FBXObjectID id : node.child_ids) {
            if (id) {
                FBXNode *child_node;

                if (GetFBXObject(id, child_node)) {
                    BuildNodes(type, *child_node, node_proxy.Get());

                    continue;
                }

                DebugLog(
                    LogType::Error,
                    "Unsure how to build child object %lld\n",
                    id
                );
            }
        }

        parent_node->AddChild(node_proxy);
#endif
    };

    auto ApplyBindPoses = [&]() {
        for (const FBXObjectID id : bind_pose_ids) {
            FBXBindPose *bind_pose;

            if (!GetFBXObject(id, bind_pose)) {
                DebugLog(
                    LogType::Warn,
                    "Not a valid bind pose node: %lld\n",
                    id
                );

                continue;
            }

            for (const FBXPoseNode &pose_node : bind_pose->pose_nodes) {
                FBXNode *linked_node;

                if (!GetFBXObject(pose_node.node_id, linked_node)) {
                    DebugLog(
                        LogType::Warn,
                        "Linked node %lld to pose node is not valid\n",
                        pose_node.node_id
                    );

                    continue;
                }

                linked_node->world_bind_matrix = pose_node.matrix;
            }
        }
    };

    std::function<void(FBXNode &)> ApplyLocalBindPose;

    ApplyLocalBindPose = [&](FBXNode &node) {
        node.local_bind_matrix = node.world_bind_matrix;

        if (node.parent_id) {
            FBXNode *parent_node;

            if (GetFBXObject(node.parent_id, parent_node)) {
                node.local_bind_matrix = parent_node->world_bind_matrix.Inverted() * node.local_bind_matrix;
            }
        }

        for (const FBXObjectID id : node.child_ids) {
            if (id) {
                FBXNode *child_node;

                if (GetFBXObject(id, child_node)) {
                    ApplyLocalBindPose(*child_node);
                }
            }
        }
    };

    auto CalculateLocalBindPoses = [&]() {
        for (const FBXObjectID id : bind_pose_ids) {
            FBXBindPose *bind_pose;

            if (!GetFBXObject(id, bind_pose)) {
                DebugLog(
                    LogType::Warn,
                    "Not a valid bind pose node: %lld\n",
                    id
                );

                continue;
            }

            for (const FBXPoseNode &pose_node : bind_pose->pose_nodes) {
                FBXNode *linked_node;

                if (!GetFBXObject(pose_node.node_id, linked_node)) {
                    DebugLog(
                        LogType::Warn,
                        "Linked node %lld to pose node is not valid\n",
                        pose_node.node_id
                    );

                    continue;
                }

                ApplyLocalBindPose(*linked_node);
            }
        }
    };

    ApplyBindPoses();
    CalculateLocalBindPoses();

    std::function<void(FBXNode &)> BuildLimbNodes;

    BuildLimbNodes = [&](FBXNode &node) {
        if (node.type == FBXNode::Type::LIMB_NODE) {
            found_first_bone = true;

            BuildNodes(FBXNode::Type::LIMB_NODE, node, root_bone.Get());
        } else {
            for (const FBXObjectID id : node.child_ids) {
                if (id) {
                    FBXNode *child_node;

                    if (GetFBXObject(id, child_node)) {
                        BuildLimbNodes(*child_node);
                    }
                }
            }
        }
    };

#if 0

    std::function<FBXNode *(FBXNode &)> FindFirstLimbNode;

    FindFirstLimbNode = [&](FBXNode &node) -> FBXNode * {
        if (node.type == FBXNode::Type::LIMB_NODE) {
            return &node;
        }

        for (const FBXObjectID id : node.child_ids) {
            if (id) {
                FBXNode *child_node;

                if (GetFBXObject(id, child_node)) {
                    if (FBXNode *limb_node = FindFirstLimbNode(*child_node)) {
                        return limb_node;
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
    BuildLimbNodes(root_fbx_node);

    for (const FBXObjectID id : root_fbx_node.child_ids) {
        if (id) {
            FBXNode *child_node;

            if (GetFBXObject(id, child_node)) {
                BuildNodes(FBXNode::Type::NODE, *child_node, top.Get());
            }
        }
    }

    if (found_first_bone) {
        // Add Skeleton and AnimationController to Entities
        //AddSkeletonToEntities(root_skeleton, top.Get());
    }

    top->UpdateWorldTransform();

    if (Skeleton *skeleton = root_skeleton.Get()) {
        if (Bone *root_bone = skeleton->GetRootBone()) {
            root_bone->SetToBindingPose();

            root_bone->CalculateBoneRotation();
            root_bone->CalculateBoneTranslation();

            root_bone->StoreBindingPose();
            root_bone->ClearPose();

            root_bone->UpdateBoneTransform();
        }
    }

    return { { LoaderResult::Status::OK }, top.Cast<void>() };
}

} // namespace hyperion::v2
